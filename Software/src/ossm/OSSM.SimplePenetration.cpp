#include "OSSM.h"

#include <queue>
#include <chrono>
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

    auto best = std::chrono::steady_clock::now();
    PositionTime lastPositionTime;

    // Motion state
    float currentPosition = 0;
    float targetPosition = 0.0f;
    bool finished = true; //for debugging when strokes finish early.
    int compensate = 70;
    float maxSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
    float maxAccel = Config::Driver::maxAcceleration * (1_mm);

    // Set initial max speed and acceleration
    ossm->stepper->setSpeedInHz((1_mm) * Config::Driver::maxSpeedMmPerSecond);
    ossm->stepper->setAcceleration((1_mm) * Config::Driver::maxAcceleration);

    while (isInCorrectState(ossm)) {
        if (ossm->stepper->isRunning()){
            vTaskDelay(1);
            continue;
        }

        int buffer = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - best).count();
        // Wait for new command from BLE
        if (targetQueue.empty()){
            if(!finished){
                finished = true;
                Serial.println("Movement Done: " + String(buffer));
            }
            vTaskDelay(1);
            continue;
        }
        finished = false;
        
        PositionTime targetPositionTime = targetQueue.front();
        targetQueue.pop();
        
        if (targetPositionTime.setTime){
            int lag = std::chrono::duration_cast<std::chrono::milliseconds>(targetPositionTime.setTime.value() - best).count();
            if (lag < 0 || lag > compensate * 10){
                best = targetPositionTime.setTime.value();
                lag = 0;
            }
            best += std::chrono::milliseconds(targetPositionTime.inTime);
            ESP_LOGI("Streaming", "Lag: %d, Buffer: %d", lag, buffer);

            int mincomp =  min(compensate,int(lastPositionTime.inTime));
            if (buffer < mincomp){
                ESP_LOGI("Streaming", "Buffer not full, waiting %dms",mincomp-buffer);
                vTaskDelay((mincomp-buffer) / portTICK_PERIOD_MS);
            } else {
                int reduction = min(targetPositionTime.inTime/4, buffer - mincomp);
                ESP_LOGI("Streaming", "Lag too great, shortening stroke by %dms", reduction);
                targetPositionTime.inTime -= reduction;
            }
            lastPositionTime = targetPositionTime;
        }

        float maxStroke = abs(((float)OSSM::setting.stroke / 100.0) * ossm->measuredStrokeSteps);
        float offset = (ossm->measuredStrokeSteps - maxStroke) * 0.5;
        targetPosition = -(1-(static_cast<float>(targetPositionTime.position) / 100.0f)) * maxStroke - offset;
        currentPosition = static_cast<float>(ossm->stepper->getCurrentPosition());

        // Calculate distance to travel (in steps)
        float distance = abs(targetPosition - currentPosition);
        float timeSeconds = targetPositionTime.inTime / 1000.0f;

        if (timeSeconds > 0.01f && distance > 1.0f) {
            float requiredSpeed = (distance / timeSeconds) * 1.5f;
            requiredSpeed = constrain(requiredSpeed, 100.0f, maxSpeed);
            float requiredAccel = int(3.0 * requiredSpeed / timeSeconds);
            requiredAccel = constrain(requiredAccel, 100.0f, maxAccel);
            ossm->stepper->setAcceleration(requiredAccel);
            ossm->stepper->setSpeedInHz(static_cast<uint32_t>(requiredSpeed));
            ossm->stepper->moveTo(static_cast<int32_t>(targetPosition), false);

            ESP_LOGI("Streaming", "P(%d): %.0f -> %.0f = %.0f, T: %.3f, S: %.0f, A: %.0f, Q: %d",
                     targetPositionTime.position, currentPosition/(1_mm), targetPosition/(1_mm), distance/(1_mm), 
                     timeSeconds, requiredSpeed/(1_mm), requiredAccel/(1_mm), targetQueue.size());
        }
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
