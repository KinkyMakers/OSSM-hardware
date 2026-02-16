#include "simple_penetration.h"

#include <queue>

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

static void startStreamingTask(void *pvParameters) {
    auto isInCorrectState = []() {
        return stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.preflight"_s) ||
               stateMachine->is("streaming.idle"_s);
    };
    // Reset to home position
    stepper->moveTo(0, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Motion state
    float currentPosition = 0;
    float targetPosition = 0.0f;

    // Set initial max speed and acceleration
    stepper->setSpeedInHz((1_mm) * Config::Driver::maxSpeedMmPerSecond);
    stepper->setAcceleration((1_mm) * Config::Driver::maxAcceleration);

    while (isInCorrectState()) {
        // Wait for new command from BLE
        if (!consumeTargetUpdate()) {
            vTaskDelay(1);
            continue;
        }

        // Calculate target position from FTS position (0-180)
        // FTS: 0 = retracted, 180 = extended
        // Stepper: 0 = home (retracted), negative = extended
        targetPosition =
            -(static_cast<float>(targetPositionTime.position) / 180.0f) *
            calibration.measuredStrokeSteps;

        currentPosition = static_cast<float>(stepper->getCurrentPosition());

        // Calculate distance to travel (in steps)
        float distance = abs(targetPosition - currentPosition);

        // Time to reach target (in seconds)
        float timeSeconds = targetPositionTime.inTime / 1000.0f;

        float maxSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
        float maxAccel = Config::Driver::maxAcceleration * (1_mm);

        // Always use maximum acceleration for responsive motion
        stepper->setAcceleration(maxAccel);

        // Calculate required speed to reach target in given time
        if (timeSeconds > 0.01f && distance > 1.0f) {
            // v = d / t (basic kinematics for constant velocity)
            // Use 1.5x to account for accel/decel phases eating into travel
            // time
            float requiredSpeed = (distance / timeSeconds) * 1.5f;

            // Clamp to safe limits
            requiredSpeed = constrain(requiredSpeed, 100.0f, maxSpeed);

            ESP_LOGI("Streaming",
                     "Pos: %d -> %.0f, Dist: %.0f, Time: %.3fs, Speed: %.0f",
                     targetPositionTime.position, targetPosition, distance,
                     timeSeconds, requiredSpeed);

            stepper->setSpeedInHz(static_cast<uint32_t>(requiredSpeed));
        } else {
            // Very short time or no distance - use max speed
            stepper->setSpeedInHz(static_cast<uint32_t>(maxSpeed));
        }

        stepper->applySpeedAcceleration();
        stepper->moveTo(static_cast<int32_t>(targetPosition), false);

        vTaskDelay(1);
    }

    vTaskDelete(nullptr);
}

void startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);
}

}  // namespace simple_penetration
