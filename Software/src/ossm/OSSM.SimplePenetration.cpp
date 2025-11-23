#include "OSSM.h"

#include <queue>

#include "constants/Config.h"
#include "services/communication/nimble.h"
#include "services/communication/queue.h"

void OSSM::startSimplePenetrationTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;

    int fullStrokeCount = 0;
    static int32_t targetPosition = 0;

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("simplePenetration"_s) ||
               ossm->sm->is("simplePenetration.idle"_s);
    };

    double lastSpeed = 0;

    bool stopped = false;

    while (isInCorrectState(ossm)) {
        auto speed = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                     OSSM::setting.speed / 100.0;
        auto acceleration = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                            OSSM::setting.speed * OSSM::setting.speed /
                            Config::Advanced::accelerationScaling;

        bool isSpeedZero = OSSM::setting.speedKnob <
                           Config::Advanced::commandDeadZonePercentage;
        bool isSpeedChanged =
            !isSpeedZero && abs(speed - lastSpeed) >
                                5 * Config::Advanced::commandDeadZonePercentage;
        bool isAtTarget =
            abs(targetPosition - ossm->stepper->getCurrentPosition()) == 0;

        // If the speed is zero, then stop the stepper and wait for the next
        if (isSpeedZero) {
            ossm->stepper->stopMove();
            stopped = true;
            vTaskDelay(100);
            continue;
        } else if (stopped) {
            ossm->stepper->moveTo(targetPosition, false);
            stopped = false;
        }

        // If the speed is greater than the dead-zone, and the speed has changed
        // by more than the dead-zone, then update the stepper.
        // This must be done in the same task that the stepper is running in.
        if (isSpeedChanged) {
            lastSpeed = speed;
            ossm->stepper->setAcceleration(acceleration);
            ossm->stepper->setSpeedInHz(speed);
        }

        // If the stepper is not at the target, then wait for the next loop
        if (!isAtTarget) {
            vTaskDelay(1);
            // more than zero
            continue;
        }

        bool nextDirection = !ossm->isForward;
        ossm->isForward = nextDirection;

        if (ossm->isForward) {
            targetPosition = -abs(((float)OSSM::setting.stroke / 100.0) *
                                  ossm->measuredStrokeSteps);
        } else {
            targetPosition = 0;
        }

        ESP_LOGV("SimplePenetration", "target: %f,\tspeed: %f,\tacc: %f",
                 targetPosition, speed, acceleration);

        ossm->stepper->moveTo(targetPosition, false);

        if (OSSM::setting.speed > Config::Advanced::commandDeadZonePercentage &&
            OSSM::setting.stroke >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            ossm->sessionStrokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            ossm->sessionDistanceMeters +=
                (((float)OSSM::setting.stroke / 100.0) *
                 ossm->measuredStrokeSteps / (1_mm)) /
                1000.0;
        }

        vTaskDelay(1);
    }

    vTaskDelete(nullptr);
}

void OSSM::startSimplePenetration() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(
        startSimplePenetrationTask, "startSimplePenetrationTask", stackSize,
        this, configMAX_PRIORITIES - 1, &Tasks::runSimplePenetrationTaskH,
        Tasks::operationTaskCore);
}

void startStreamingTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;

    auto isInCorrectState = [](OSSM *ossm) {
        return ossm->sm->is("streaming"_s) ||
               ossm->sm->is("streaming.preflight"_s) ||
               ossm->sm->is("streaming.idle"_s);
    };
    // Reset to home position
    ossm->stepper->moveTo(0, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Motion state
    float currentPosition = 0;
    float currentVelocity = 0;

    float targetPosition = 0.0f;
    float targetVelocity = 0.0f;

    // set stepper to max speed and max acceleration
    ossm->stepper->setSpeedInHz((1_mm) * Config::Driver::maxSpeedMmPerSecond);
    ossm->stepper->setAcceleration((1_mm) * Config::Driver::maxAcceleration);

    // Speed multipliers for different speed levels
    const float speedMultipliers[] = {10.0, 11.0, 12.0, 13.0, 14.0, 15.0};
    int speedMultiplierIndex = 0;

    float RMS_position = 0;
    int rms_samples = 0;

    int lastTime = 0;
    long loopStart = 0;

    while (isInCorrectState(ossm)) {
        // Calculate target position
        targetPosition =
            -(constrain(static_cast<float>(180 - targetPositionTime.position),
                        0.0f, 180.0f) /
              180.0f) *
            ossm->measuredStrokeSteps;

        ESP_LOGD("Streaming", "%d, %d, %d", ossm->stepper->getCurrentPosition(),
                 static_cast<int32_t>(targetPosition),
                 targetPositionTime.inTime);
        if (!hasTargetChanged()) {
            vTaskDelay(1);
            continue;
        }

        currentPosition =
            static_cast<float>(ossm->stepper->getCurrentPosition());
        currentVelocity =
            static_cast<float>(ossm->stepper->getCurrentSpeedInMilliHz());

        // Kinematic calculation: Always use max acceleration, compute required
        // velocity Convert units: velocity from milliHz to Hz, time from ms to
        // seconds
        float v0 = currentVelocity / 1000.0f;  // Current velocity in steps/s
        float deltaX =
            targetPosition - currentPosition;           // Displacement in steps
        float t = targetPositionTime.inTime / 1000.0f;  // Time in seconds

        float maxSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
        float maxAccel = Config::Driver::maxAcceleration * (1_mm);

        // Always use maximum acceleration to guarantee we can meet timing
        ossm->stepper->setAcceleration(maxAccel);

        // Handle edge cases
        if (t <= 0.001f) {
            // If time is too small, use maximum speed
            targetVelocity = maxSpeed;
        } else {
            // Calculate required velocity using: deltaX = (v0 + v1)/2 * t
            // Solving for v1: v1 = 2*deltaX/t - v0
            targetVelocity = 2.0f * deltaX / t - v0;

            // Take absolute value and clamp to max speed
            targetVelocity = maxSpeed;
        }

        ossm->stepper->setSpeedInHz(maxSpeed);
        ossm->stepper->applySpeedAcceleration();

        vTaskDelay(1);
        ossm->stepper->moveTo(targetPosition, false);
        vTaskDelay(1);
    }

    vTaskDelete(nullptr);
}

void OSSM::startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            this, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);
}
