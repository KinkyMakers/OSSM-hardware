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
constexpr int32_t kWiggleDistanceSteps = 5_mm;
constexpr uint32_t kWiggleTimeoutMs = 1500;
constexpr uint32_t kCurrentLimitSettleMs = 50;
constexpr float kProbeSpeedStepsPerSecond = 10_mm;
constexpr float kProbeAccelerationStepsPerSecondSquared = 1000_mm;
// Restore the established full-stroke homing speed. The current-limited
// hard-stop wiggle remains independently capped at 10 mm/s.
constexpr float kSafeHomingSpeedStepsPerSecond = 25_mm;
constexpr float kSafeHomingAccelerationStepsPerSecondSquared = 100_mm;
constexpr float kMinimumProbeSignal = 0.05f;
constexpr float kProbeTieMargin = 0.05f;
constexpr float kWiggleCurrentLimit = 0.75f;
constexpr uint8_t kContactConfirmationSamples = 3;

float homingCurrentLimit = Config::Driver::sensorlessCurrentLimit;

struct CurrentProbe {
    float averageLoad = 0;
    float peakLoad = 0;
    float completionRatio = 0;
    uint32_t elapsedMs = 0;
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
                             float speedStepsPerSecond, uint32_t timeoutMs,
                             float currentLimit,
                             uint8_t confirmationSamples = 1,
                             uint32_t currentLimitSettleMs = 0) {
    CurrentProbe result;
    stepper->setAcceleration(kProbeAccelerationStepsPerSecondSquared);
    stepper->setSpeedInHz(speedStepsPerSecond);
    const int32_t startedPosition = stepper->getCurrentPosition();
    const int32_t target = startedPosition + sign * distanceSteps;
    const int32_t requestedDistance = std::abs(target - startedPosition);
    stepper->moveTo(target, false);

    const uint32_t started = millis();
    uint32_t sampleCount = 0;
    uint8_t samplesOverLimit = 0;
    double loadTotal = 0;
    while (true) {
        const float load = readMotorLoadPercent();
        loadTotal += load;
        ++sampleCount;

        const uint32_t elapsedMs = millis() - started;
        if (elapsedMs >= currentLimitSettleMs) {
            result.peakLoad = std::max(result.peakLoad, load);
        }
        if (elapsedMs >= currentLimitSettleMs && load >= currentLimit) {
            if (++samplesOverLimit >= confirmationSamples) {
                result.hitHardLimit = true;
                break;
            }
        } else {
            samplesOverLimit = 0;
        }
        if (!stepper->isRunning()) break;
        if (elapsedMs >= timeoutMs) {
            result.timedOut = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    const int32_t reportedDistance =
        std::abs(stepper->getCurrentPosition() - startedPosition);
    result.completionRatio =
        requestedDistance > 0
            ? std::min(1.0f,
                       static_cast<float>(reportedDistance) /
                           static_cast<float>(requestedDistance))
            : 1.0f;
    result.elapsedMs = millis() - started;
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

bool probeAndEscapeHardStopImpl(ProbeDiagnostics* diagnostics) {
    // Travel from -5 mm to +5 mm around one fixed origin. The first leg is
    // 5 mm and the second is 10 mm, for exactly 15 mm of commanded wiggle
    // travel when neither side is current-limited.
    const int32_t wiggleOrigin = stepper->getCurrentPosition();
    const homing_logic::WiggleTargets wiggleTargets =
        homing_logic::calculateWiggleTargets(
            wiggleOrigin, static_cast<uint32_t>(kWiggleDistanceSteps));
    const CurrentProbe negative = runCurrentProbe(
        -1, wiggleOrigin - wiggleTargets.negative,
        kProbeSpeedStepsPerSecond,
        kWiggleTimeoutMs, kWiggleCurrentLimit, kContactConfirmationSamples,
        kCurrentLimitSettleMs);
    const int32_t positiveDistance =
        wiggleTargets.positive - stepper->getCurrentPosition();
    const CurrentProbe positive = runCurrentProbe(
        1, positiveDistance, kProbeSpeedStepsPerSecond,
        kWiggleTimeoutMs * 2, kWiggleCurrentLimit,
        kContactConfirmationSamples,
        kCurrentLimitSettleMs);
    const homing_logic::ProbeDirection direction =
        homing_logic::chooseWiggleEscapeDirection(
            negative.averageLoad, positive.averageLoad, negative.hitHardLimit,
            positive.hitHardLimit, kMinimumProbeSignal, kProbeTieMargin,
            homing_logic::ProbeDirection::Negative);
    homingCurrentLimit = kWiggleCurrentLimit;

    if (diagnostics != nullptr) {
        diagnostics->negativeAverageLoad = negative.averageLoad;
        diagnostics->negativePeakLoad = negative.peakLoad;
        diagnostics->negativeCompletionRatio = negative.completionRatio;
        diagnostics->negativeElapsedMs = negative.elapsedMs;
        diagnostics->positiveAverageLoad = positive.averageLoad;
        diagnostics->positivePeakLoad = positive.peakLoad;
        diagnostics->positiveCompletionRatio = positive.completionRatio;
        diagnostics->positiveElapsedMs = positive.elapsedMs;
        diagnostics->direction = static_cast<int8_t>(direction);
        diagnostics->adaptiveCurrentLimit = homingCurrentLimit;
        diagnostics->negativeHitHardLimit = negative.hitHardLimit;
        diagnostics->positiveHitHardLimit = positive.hitHardLimit;
        diagnostics->negativeTimedOut = negative.timedOut;
        diagnostics->positiveTimedOut = positive.timedOut;
    }

    ESP_LOGI(
        "Homing",
        "HOMING_PROBE origin=%d positive_target=%d limit=%.3f "
        "negative_avg=%.3f negative_peak=%.3f negative_completion=%.3f "
        "negative_ms=%u positive_avg=%.3f positive_peak=%.3f "
        "positive_completion=%.3f positive_ms=%u direction=%d "
        "negative_blocked=%d positive_blocked=%d negative_timeout=%d "
        "positive_timeout=%d",
        wiggleOrigin, wiggleTargets.positive, homingCurrentLimit,
        negative.averageLoad, negative.peakLoad, negative.completionRatio,
        negative.elapsedMs, positive.averageLoad, positive.peakLoad,
        positive.completionRatio, positive.elapsedMs,
        static_cast<int>(direction),
        negative.hitHardLimit, positive.hitHardLimit, negative.timedOut,
        positive.timedOut);

    if (negative.timedOut || positive.timedOut ||
        direction == homing_logic::ProbeDirection::Unsafe) {
        return false;
    }
    return true;
}

}  // namespace

bool probeAndEscapeHardStop(ProbeDiagnostics* diagnostics) {
    return probeAndEscapeHardStopImpl(diagnostics);
}

void clearHoming() {
    ESP_LOGD("Homing", "Homing started");

    // Set homing active flag for LED indication
    setHomingActive(true);

    calibration.isForward = true;
    homingCurrentLimit = Config::Driver::sensorlessCurrentLimit;

    // Set acceleration and deceleration in steps/s^2
    stepper->setAcceleration(kSafeHomingAccelerationStepsPerSecondSquared);
    // Set speed in steps/s
    stepper->setSpeedInHz(kSafeHomingSpeedStepsPerSecond);

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
    stepper->setAcceleration(kSafeHomingAccelerationStepsPerSecondSquared);
    stepper->setSpeedInHz(kSafeHomingSpeedStepsPerSecond);
    int16_t sign = stateMachine->is("homing.backward"_s) ? 1 : -1;

    int32_t targetPositionInSteps =
        round(sign * Config::Driver::maxStrokeSteps);

    ESP_LOGD("Homing", "Target position in steps: %d", targetPositionInSteps);
    stepper->moveTo(targetPositionInSteps, false);
    const uint32_t homingMoveStartedMs = millis();

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("homing"_s) ||
               stateMachine->is("homing.forward"_s) ||
               stateMachine->is("homing.backward"_s);
    };

    // Require a sustained current rise so a single ADC transient cannot be
    // mistaken for a hard stop.
    uint8_t currentSamplesOverLimit = 0;
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
        if (millis() - homingMoveStartedMs < kCurrentLimitSettleMs) {
            vTaskDelay(10);
            continue;
        }
        bool isCurrentOverLimit = homing_logic::isCurrentOverLimit(
            current, 0, homingCurrentLimit);

        if (!isCurrentOverLimit) {
            currentSamplesOverLimit = 0;
            vTaskDelay(10);  // Increased from 1ms to 10ms to reduce CPU load
            continue;
        }
        if (++currentSamplesOverLimit < kContactConfirmationSamples) {
            vTaskDelay(10);
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
