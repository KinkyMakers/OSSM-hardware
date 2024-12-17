#include "OSSM.h"

#include "constants/Config.h"

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

    // PID constants - these may need tuning
    const float Kp = 0.8;  // Proportional gain
    const float Ki = 0.2;  // Integral gain
    const float Kd = 0.1;  // Derivative gain

    float integral = 0;
    float lastError = 0;
    float lastPosition = 0;
    uint32_t lastTime = millis();

    auto sineWave = [](int time, float strokeSteps) {
        return -0.5 * (sin(time * 2 * M_PI / 5000) + 1) * strokeSteps;
    };

    while (isInCorrectState(ossm)) {
        uint32_t currentTime = millis();
        float deltaTime =
            (currentTime - lastTime) / 1000.0;  // Convert to seconds

        // Calculate target position
        float targetPosition = sineWave(currentTime, ossm->measuredStrokeSteps);
        float currentPosition = ossm->stepper->getCurrentPosition();
        float currentSpeed = ossm->stepper->getCurrentSpeedInMilliHz();

        // Calculate PID terms
        float error = targetPosition - currentPosition;
        integral += error * deltaTime;
        float derivative = (error - lastError) / deltaTime;

        // Calculate PID output (speed adjustment)
        float pidOutput = (Kp * error) + (Ki * integral) + (Kd * derivative);

        // Convert PID output to speed (Hz)
        float speedInMilliHz =
            abs(pidOutput) * 1000;  // Scale factor may need adjustment

        // Apply limits to speed
        speedInMilliHz =
            min(speedInMilliHz,
                1000 * Config::Driver::maxSpeedMmPerSecond * (1_mm));

        // Update stepper
        ossm->stepper->setSpeedInMilliHz(speedInMilliHz);
        ossm->stepper->moveTo(targetPosition, false);

        // Debug logging
        ESP_LOGI("Streaming", "%f, %f, %f, %f, %f", targetPosition,
                 currentPosition, error, speedInMilliHz, currentSpeed);

        // Update state for next iteration
        lastError = error;
        lastTime = currentTime;

        vTaskDelay(pdMS_TO_TICKS(1000 / 60));  // Process at 60Hz
    }

    vTaskDelete(nullptr);
}

void OSSM::startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            this, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);
}