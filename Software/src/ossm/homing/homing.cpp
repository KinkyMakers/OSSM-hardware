#include "homing.h"

#include "Strings.h"
#include "constants/Config.h"
#include "constants/Pins.h"
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

    void clearHoming() {
        ESP_LOGD("Homing", "Homing started");

        // Set homing active flag for LED indication
        setHomingActive(true);

        // Set acceleration and deceleration in steps/s^2
        stepper->setAcceleration(1000_mm);
        // Set speed in steps/s
        stepper->setSpeedInHz(25_mm);

        // Recalibrate the current sensor offset.
        calibration.currentSensorOffset = (getAnalogAveragePercent(SampleOnPin{Pins::Driver::currentSensorPin, 1000}));
    }

    static void startHomingTask(void *pvParameters) {
        TickType_t xTaskStartTime = xTaskGetTickCount();

#ifdef AJ_DEVELOPMENT_HARDWARE
        stepper->setCurrentPosition(0);
        stepper->forceStopAndNewPosition(0);
        stateMachine->process_event(Done{});
        vTaskDelete(nullptr);
        return;
#endif

        // Stroke Engine and Simple Penetration treat this differently.
        stepper->enableOutputs();
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
        int16_t sign = stateMachine->is("homing.backward"_s) ? 1 : -1;

        int32_t targetPositionInSteps = round(sign * Config::Driver::maxStrokeSteps);

        ESP_LOGD("Homing", "Target position in steps: %d", targetPositionInSteps);
        stepper->moveTo(targetPositionInSteps, false);

        auto isInCorrectState = []() {
            // Add any states that you want to support here.
            return stateMachine->is("homing"_s) || stateMachine->is("homing.forward"_s) || stateMachine->is("homing.backward"_s);
        };

        bool second = false;
        while (isInCorrectState()) {
            TickType_t xCurrentTickCount = xTaskGetTickCount();
            TickType_t xTicksPassed = xCurrentTickCount - xTaskStartTime;
            uint32_t msPassed = xTicksPassed * portTICK_PERIOD_MS;
            if (msPassed > 40000) {
                ESP_LOGE("Homing", "Homing took too long. Check power and restart");
                errorState.message = ui::strings::homingTookTooLong;

                setHomingActive(false);
                stateMachine->process_event(Error{});
                break;
            }

            float current = getAnalogAveragePercent(SampleOnPin{Pins::Driver::currentSensorPin, 50}) - calibration.currentSensorOffset;

            ESP_LOGV("Homing", "Current: %f", current);
            bool isCurrentOverLimit = current > Config::Driver::sensorlessCurrentLimit;

            if (!isCurrentOverLimit) {
                vTaskDelay(10);  // Increased from 1ms to 10ms to reduce CPU load
                continue;
            }

            ESP_LOGD("Homing", "Current over limit: %f", current);
            stepper->stopMove();

            int32_t currentPosition = stepper->getCurrentPosition();
            stepper->moveTo(currentPosition - sign * Config::Driver::homingOffsetMn, true);

            if (!second && stateMachine->is("homing.backward"_s) && isStrokeTooShort()) {
                second = true;
                stepper->moveTo(targetPositionInSteps, false);
                continue;
            }

            // measure and save the current position
            calibration.measuredStrokeSteps = max(float(abs(stepper->getCurrentPosition())), calibration.measuredStrokeSteps);
            calibration.measuredStrokeSteps = min(Config::Driver::maxStrokeSteps, calibration.measuredStrokeSteps);

            ESP_LOGI("Homing", "Steps: %f", calibration.measuredStrokeSteps);

            if (stateMachine->is("homing.backward"_s)) {
                stepper->setCurrentPosition(0);
                stepper->forceStopAndNewPosition(0);
            }

            stepper->setSpeedInHz(250_mm);
            stepper->moveTo(0, true);

            // Clear homing active flag for LED indication
            setHomingActive(false);

            stateMachine->process_event(Done{});
            break;
        };

        vTaskDelete(nullptr);
    }

    void startHoming() {
        int stackSize = 10 * configMINIMAL_STACK_SIZE;
        xTaskCreatePinnedToCore(startHomingTask, "startHomingTask", stackSize, nullptr, configMAX_PRIORITIES - 1, &Tasks::runHomingTaskH,
                                Tasks::operationTaskCore);
    }

    bool isStrokeTooShort() {
        if (calibration.measuredStrokeSteps > Config::Driver::minStrokeLengthMm) {
            return false;
        }
        errorState.message = ui::strings::strokeTooShort;
        return true;
    }

}  // namespace homing
