#include "OSSM.h"

#include <queue>

#include "constants/Config.h"
#include "services/nimble.h"

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
                     ossm->setting.speed / 100.0;
        auto acceleration = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                            ossm->setting.speed * ossm->setting.speed /
                            Config::Advanced::accelerationScaling;

        bool isSpeedZero = ossm->setting.speedKnob <
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
            targetPosition = -abs(((float)ossm->setting.stroke / 100.0) *
                                  ossm->measuredStrokeSteps);
        } else {
            targetPosition = 0;
        }

        ESP_LOGV("SimplePenetration", "target: %f,\tspeed: %f,\tacc: %f",
                 targetPosition, speed, acceleration);

        ossm->stepper->moveTo(targetPosition, false);

        if (ossm->setting.speed > Config::Advanced::commandDeadZonePercentage &&
            ossm->setting.stroke >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            ossm->sessionStrokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            ossm->sessionDistanceMeters +=
                (((float)ossm->setting.stroke / 100.0) *
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

    const int processFrequencyHz = 30;
    const int processTimeMs = 1000 / processFrequencyHz;

    int32_t targetPositionSteps = 0;
    float lastSpeed = 0;
    int32_t lastTargetPositionSteps = 99999;
    bool stopped = false;

    // Set lower acceleration/deceleration for smoother motion
    ossm->stepper->setAcceleration((1_mm) * Config::Driver::maxAcceleration /
                                   5);  // 25% of max
    while (isInCorrectState(ossm)) {
        // if we're not at the target position, then move to it
        targetPositionSteps =
            -abs(((static_cast<float>(ossm->targetPosition)) / 100.0) *
                 ossm->measuredStrokeSteps);

        bool isTargetChanged = targetPositionSteps != lastTargetPositionSteps;
        lastTargetPositionSteps = targetPositionSteps;

        bool isAtTarget =
            abs(targetPositionSteps - ossm->stepper->getCurrentPosition()) == 0;

        if (isTargetChanged) {
            ESP_LOGD("Streaming", "Moving to target position: %d",
                     targetPositionSteps);
            ossm->stepper->moveTo(targetPositionSteps, false);
        }

        vTaskDelay(1);
    }

    // keep this queue code... but we're not using it right now.
    while (isInCorrectState(ossm)) {
        loopStart = millis();

        // Get next target from queue if available
        if (!targetQueue.empty()) {
            auto nextTarget = targetQueue.front();
            targetQueue.pop();
            nimbleTargetPosition = nextTarget.position;
        } else {
            ESP_LOGD("Streaming", "No target found. Queue is empty.");
            // Read about 20 times per second
            vTaskDelay(pdMS_TO_TICKS(processTimeMs));
            continue;
        }

        currentPosition =
            static_cast<float>(ossm->stepper->getCurrentPosition());
        currentVelocity =
            static_cast<float>(ossm->stepper->getCurrentSpeedInMilliHz());

        // Calculate target position and velocity
        targetPosition =
            -(static_cast<float>(nimbleTargetPosition) / INT16_MAX) *
            ossm->measuredStrokeSteps;

        // float speedMultiplier = speedMultipliers[speedMultiplierIndex];

        targetVelocity =
            256 * (targetPosition - currentPosition) / processTimeMs;

        ESP_LOGD("Streaming",
                 "targetPosition: %f, targetVelocity: %f, "
                 "currentPosition: %f, currentVelocity: %f",
                 targetPosition, targetVelocity, currentPosition,
                 currentVelocity);

        RMS_position += abs(targetPosition - currentPosition);
        rms_samples++;

        ossm->stepper->setSpeedInHz(abs(targetVelocity));
        ossm->stepper->moveTo(targetPosition, false);
        ossm->stepper->applySpeedAcceleration();

        int waitTime = processTimeMs - (millis() - loopStart);
        if (waitTime > 0) {
            vTaskDelay(pdMS_TO_TICKS(waitTime));
        } else {
            // Read about 20 times per second
            vTaskDelay(1);
        }
    }

    vTaskDelete(nullptr);
}

void OSSM::startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            this, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);
}
