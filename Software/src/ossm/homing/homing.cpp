#include "homing.h"

#include "Strings.h"
#include "homing_logic.h"
#include "constants/Config.h"
#include "constants/Pins.h"
#include "constants/UserConfig.h"
#include "ossm/Events.h"
#include "ossm/state/calibration.h"
#include "ossm/state/error.h"
#include "ossm/state/state.h"
#include "services/led.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "utils/analog.h"

namespace sml = boost::sml;
using namespace sml;

namespace homing {

namespace {

constexpr int kHomingTaskStackSize = 5 * configMINIMAL_STACK_SIZE;
constexpr int32_t kProbeDistanceSteps = 0.5_mm;
constexpr int32_t kEscapeDistanceSteps = 5_mm;
constexpr uint32_t kProbeTimeoutMs = 500;
constexpr uint32_t kEscapeTimeoutMs = 1500;
constexpr float kProbeSpeedStepsPerSecond = 2_mm;
constexpr float kEscapeSpeedStepsPerSecond = 5_mm;
constexpr float kProbeAccelerationStepsPerSecondSquared = 100_mm;
constexpr float kMinimumProbeSignal = 0.15f;
constexpr float kProbeTieMargin = 0.25f;
constexpr float kRequiredContactRise = 1.5f;

float homingCurrentLimit = Config::Driver::sensorlessCurrentLimit;

struct CurrentProbe {
    float averageLoad = 0;
    float peakLoad = 0;
    bool hitHardLimit = false;
    bool timedOut = false;
};

float readMotorLoadPercent() {
    return std::abs(
        getAnalogAveragePercent(
            SampleOnPin{Pins::Driver::currentSensorPin, 10}) -
        calibration.currentSensorOffset);
}

void stopAtReportedPosition() {
    const int32_t position = stepper->getCurrentPosition();
    stepper->forceStop();
    stepper->forceStopAndNewPosition(position);
}

CurrentProbe runCurrentProbe(int8_t sign, int32_t distanceSteps,
                             float speedStepsPerSecond, uint32_t timeoutMs) {
    CurrentProbe result;
    stepper->setAcceleration(kProbeAccelerationStepsPerSecondSquared);
    stepper->setSpeedInHz(speedStepsPerSecond);
    const int32_t target =
        stepper->getCurrentPosition() + sign * distanceSteps;
    stepper->moveTo(target, false);

    const uint32_t started = millis();
    uint32_t sampleCount = 0;
    double loadTotal = 0;
    while (true) {
        const float load = readMotorLoadPercent();
        loadTotal += load;
        ++sampleCount;
        result.peakLoad = std::max(result.peakLoad, load);

        if (load >= Config::Driver::sensorlessCurrentLimit) {
            result.hitHardLimit = true;
            break;
        }
        if (!stepper->isRunning()) break;
        if (millis() - started >= timeoutMs) {
            result.timedOut = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    stopAtReportedPosition();
    result.averageLoad = sampleCount > 0 ? loadTotal / sampleCount : 0;
    vTaskDelay(pdMS_TO_TICKS(50));
    return result;
}

void failHomingStopped(
    const char* reason,
    const char* message = ui::strings::homingTookTooLong) {
    if (stepper != nullptr) {
        stepper->forceStop();
        stepper->disableOutputs();
    }
    calibration.isHomed = false;
    setHomingActive(false);
    errorState.message = message;
    ESP_LOGE("Homing", "HOMING_ERROR type=%s stack_bytes=%d", reason,
             kHomingTaskStackSize);
    stateMachine->process_event(Error{});
}

bool probeAndEscapeHardStopImpl() {
    const CurrentProbe negative = runCurrentProbe(
        -1, kProbeDistanceSteps, kProbeSpeedStepsPerSecond, kProbeTimeoutMs);
    const CurrentProbe positive = runCurrentProbe(
        1, kProbeDistanceSteps, kProbeSpeedStepsPerSecond, kProbeTimeoutMs);
    const homing_logic::ProbeDirection direction =
        homing_logic::chooseProbeEscapeDirection(
            negative.averageLoad, positive.averageLoad,
            Config::Driver::sensorlessCurrentLimit, kMinimumProbeSignal,
            kProbeTieMargin);
    homingCurrentLimit = homing_logic::adaptiveCurrentLimit(
        negative.averageLoad, positive.averageLoad,
        Config::Driver::sensorlessCurrentLimit, kRequiredContactRise);

    ESP_LOGI(
        "Homing",
        "HOMING_PROBE negative_avg=%.3f negative_peak=%.3f "
        "positive_avg=%.3f positive_peak=%.3f direction=%d limit=%.3f "
        "negative_timeout=%d positive_timeout=%d",
        negative.averageLoad, negative.peakLoad, positive.averageLoad,
        positive.peakLoad, static_cast<int>(direction), homingCurrentLimit,
        negative.timedOut, positive.timedOut);

    if (negative.timedOut || positive.timedOut ||
        direction == homing_logic::ProbeDirection::Unsafe) {
        return false;
    }

    const int8_t sign = static_cast<int8_t>(direction);
    const CurrentProbe escape = runCurrentProbe(
        sign, kEscapeDistanceSteps, kEscapeSpeedStepsPerSecond,
        kEscapeTimeoutMs);
    ESP_LOGI("Homing",
             "HOMING_ESCAPE direction=%d average=%.3f peak=%.3f "
             "hard_limit=%d timeout=%d",
             sign, escape.averageLoad, escape.peakLoad, escape.hitHardLimit,
             escape.timedOut);
    return !escape.hitHardLimit && !escape.timedOut &&
           escape.averageLoad < homingCurrentLimit;
}

}  // namespace

bool probeAndEscapeHardStop() { return probeAndEscapeHardStopImpl(); }

void clearHoming() {
    ESP_LOGD("Homing", "Homing started");

    // Set homing active flag for LED indication
    setHomingActive(true);

    calibration.isForward = true;
    homingCurrentLimit = Config::Driver::sensorlessCurrentLimit;

    // Set acceleration and deceleration in steps/s^2
    stepper->setAcceleration(1000_mm);
    // Set speed in steps/s
    stepper->setSpeedInHz(25_mm);

    // Clear the stored values.
    calibration.measuredStrokeSteps = 0;

    // Recalibrate the current sensor offset.
    calibration.currentSensorOffset = (getAnalogAveragePercent(
        SampleOnPin{Pins::Driver::currentSensorPin, 1000}));
}

static void startHomingTask(void *pvParameters) {
    TickType_t xTaskStartTime = xTaskGetTickCount();

#ifdef AJ_DEVELOPMENT_HARDWARE
    stepper->setCurrentPosition(0);
    stepper->forceStopAndNewPosition(0);
    stepperFrame = StepperFrame::Native;  // counter re-zeroed at home rest
    stateMachine->process_event(Done{});
    vTaskDelete(nullptr);
    return;
#endif

    // Stroke Engine and Simple Penetration treat this differently.
    stepper->enableOutputs();
    stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
    if (stateMachine->is("homing.forward"_s) &&
        !probeAndEscapeHardStop()) {
        failHomingStopped("direction_probe", ui::strings::homingProbeFailed);
        if (Tasks::runHomingTaskH == xTaskGetCurrentTaskHandle()) {
            Tasks::runHomingTaskH = nullptr;
        }
        vTaskDelete(nullptr);
        return;
    }
    stepper->setAcceleration(1000_mm);
    stepper->setSpeedInHz(25_mm);
    int16_t sign = stateMachine->is("homing.backward"_s) ? 1 : -1;

    int32_t targetPositionInSteps =
        round(sign * Config::Driver::maxStrokeSteps);

    ESP_LOGD("Homing", "Target position in steps: %d", targetPositionInSteps);
    stepper->moveTo(targetPositionInSteps, false);

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("homing"_s) ||
               stateMachine->is("homing.forward"_s) ||
               stateMachine->is("homing.backward"_s);
    };

    // run loop for 15second or until loop exits
    while (isInCorrectState()) {
        TickType_t xCurrentTickCount = xTaskGetTickCount();
        // Calculate the time in ticks that the task has been running.
        TickType_t xTicksPassed = xCurrentTickCount - xTaskStartTime;

        // If you need the time in milliseconds, convert ticks to milliseconds.
        // 'portTICK_PERIOD_MS' is the number of milliseconds per tick.
        uint32_t msPassed = xTicksPassed * portTICK_PERIOD_MS;

        if (homing_logic::isHomingTimedOut(msPassed, 40000)) {
            ESP_LOGE("Homing", "Homing took too long. Check power and restart");
            failHomingStopped("timeout");
            break;
        }

        // measure the current analog value.
        float current = readMotorLoadPercent();

        ESP_LOGV("Homing", "Current: %f", current);
        bool isCurrentOverLimit = homing_logic::isCurrentOverLimit(
            current, 0, homingCurrentLimit);

        if (!isCurrentOverLimit) {
            vTaskDelay(10);  // Increased from 1ms to 10ms to reduce CPU load
            continue;
        }

        ESP_LOGD("Homing", "Current over limit: %f", current);
        stepper->stopMove();

        stepper->setSpeedInHz(250_mm);
        // step away from the hard stop, with your hands in the air!
        int32_t currentPosition = stepper->getCurrentPosition();
        stepper->moveTo(currentPosition - sign * Config::Driver::homingOffsetMn,
                        true);

        // measure and save the current position
        calibration.measuredStrokeSteps =
            homing_logic::calculateMeasuredStroke(
                stepper->getCurrentPosition(),
                Config::Driver::maxStrokeSteps);

        stepper->setCurrentPosition(0);
        stepper->forceStopAndNewPosition(0);
        // The counter was just re-zeroed at the homed rest position: the
        // shared frame is Native again by definition.
        stepperFrame = StepperFrame::Native;

        int32_t goToPosition = homing_logic::calculatePostHomingPosition(
            sign, calibration.measuredStrokeSteps,
            UserConfig::afterHomingPosition);

        stepper->moveTo(goToPosition,true);

        // Clear homing active flag for LED indication
        setHomingActive(false);

        stateMachine->process_event(Done{});
        break;
    };

    if (Tasks::runHomingTaskH == xTaskGetCurrentTaskHandle()) {
        Tasks::runHomingTaskH = nullptr;
    }
    vTaskDelete(nullptr);
}

void startHoming() {
    // Forward completion transitions directly into backward homing before the
    // first task has deleted itself. Two 7.5 KiB stacks therefore had to
    // coexist briefly; the second allocation could fail and strand the state
    // machine in homing.backward. Use the measured-safe 3.8 KiB stack and
    // fail with motor outputs disabled when allocation is unavailable.
    TaskHandle_t task = nullptr;
    const BaseType_t created = xTaskCreatePinnedToCore(
        startHomingTask, "startHomingTask", kHomingTaskStackSize, nullptr,
        configMAX_PRIORITIES - 1, &task, Tasks::operationTaskCore);
    if (created != pdPASS) {
        failHomingStopped("task_start");
        return;
    }
    Tasks::runHomingTaskH = task;
}

bool isStrokeTooShort() {
    if (calibration.measuredStrokeSteps > Config::Driver::minStrokeLengthMm) {
        return false;
    }
    errorState.message = ui::strings::strokeTooShort;
    return true;
}

}  // namespace homing
