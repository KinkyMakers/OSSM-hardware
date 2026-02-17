#include "simple_penetration.h"

#include "constants/Config.h"
#include "ossm/state/calibration.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/communication/nimble.h"
#include "services/communication/queue.h"
#include "services/stepper.h"
#include "services/tasks.h"

namespace sml = boost::sml;
using namespace sml;

namespace simple_penetration {

static void startSimplePenetrationTask(void *pvParameters) {
    int fullStrokeCount = 0;
    static int32_t targetPosition = 0;

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("simplePenetration"_s) ||
               stateMachine->is("simplePenetration.idle"_s);
    };

    double lastSpeed = 0;

    bool stopped = false;

    while (isInCorrectState()) {
        auto speed =
            (1_mm) * Config::Driver::maxSpeedMmPerSecond * settings.speed / 100.0;
        auto acceleration = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                            settings.speed * settings.speed /
                            Config::Advanced::accelerationScaling;

        bool isSpeedZero =
            settings.speedKnob < Config::Advanced::commandDeadZonePercentage;
        bool isSpeedChanged =
            !isSpeedZero && abs(speed - lastSpeed) >
                                5 * Config::Advanced::commandDeadZonePercentage;
        bool isAtTarget =
            abs(targetPosition - stepper->getCurrentPosition()) == 0;

        // If the speed is zero, then stop the stepper and wait for the next
        if (isSpeedZero) {
            stepper->stopMove();
            stopped = true;
            vTaskDelay(100);
            continue;
        } else if (stopped) {
            stepper->moveTo(targetPosition, false);
            stopped = false;
        }

        // If the speed is greater than the dead-zone, and the speed has changed
        // by more than the dead-zone, then update the stepper.
        // This must be done in the same task that the stepper is running in.
        if (isSpeedChanged) {
            lastSpeed = speed;
            stepper->setAcceleration(acceleration);
            stepper->setSpeedInHz(speed);
        }

        // If the stepper is not at the target, then wait for the next loop
        if (!isAtTarget) {
            vTaskDelay(1);
            // more than zero
            continue;
        }

        bool nextDirection = !calibration.isForward;
        calibration.isForward = nextDirection;

        if (calibration.isForward) {
            targetPosition = -abs(((float)settings.stroke / 100.0) *
                                  calibration.measuredStrokeSteps);
        } else {
            targetPosition = 0;
        }

        ESP_LOGV("SimplePenetration", "target: %f,\tspeed: %f,\tacc: %f",
                 targetPosition, speed, acceleration);

        stepper->moveTo(targetPosition, false);

        if (settings.speed > Config::Advanced::commandDeadZonePercentage &&
            settings.stroke >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            session.strokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            session.distanceMeters +=
                (((float)settings.stroke / 100.0) *
                 calibration.measuredStrokeSteps / (1_mm)) /
                1000.0;
        }

        vTaskDelay(1);
    }

    vTaskDelete(nullptr);
}

void startSimplePenetration() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startSimplePenetrationTask,
                            "startSimplePenetrationTask", stackSize, nullptr,
                            configMAX_PRIORITIES - 1,
                            &Tasks::runSimplePenetrationTaskH,
                            Tasks::operationTaskCore);
}

}  // namespace simple_penetration
