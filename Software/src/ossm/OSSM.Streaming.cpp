#include "OSSM.h"

#include <queue>
#include <chrono>
#include "constants/Config.h"
#include "services/communication/nimble.h"
#include "services/communication/queue.h"

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

    //TODO: make these settings adjustable via remote/etc.
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
        
        // settime is when the message was received. If we trust the source we can reduce perceived lag by creating a buffer.
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
        } else {
            best = std::chrono::steady_clock::now();
        }

        float maxStroke = abs(((float)OSSM::setting.stroke / 100.0) * ossm->measuredStrokeSteps);
        // TODO: Once this is more stable, set this to max depth-stroke instead. Currently centered to avoid crashing while testing.
        float offset = (ossm->measuredStrokeSteps - maxStroke) * 0.5;
        targetPosition = -(1-(static_cast<float>(targetPositionTime.position) / 100.0f)) * maxStroke - offset;
        currentPosition = static_cast<float>(ossm->stepper->getCurrentPosition());

        // Calculate distance to travel (in steps)
        float distance = abs(targetPosition - currentPosition);
        float timeSeconds = targetPositionTime.inTime / 1000.0f;
        if (timeSeconds > 0.01f && distance > 1.0f) {
            //calculate max distance possible to travel given available time, max acceleration, and unlimited speed.
            float maxDistance = maxAccel * (timeSeconds/2) * (timeSeconds/2);
            //if the distance asked for is greater then the maximum possible, reduce the ask.
            //Is it what they asked for? No. Will they notice at these speeds? Also no.
            if (distance > maxDistance){
                ESP_LOGI("Streaming","Too fast, shortening distance: %.0f -> %.0f",distance,maxDistance);
                distance = maxDistance - (2_mm);
                if (targetPosition > currentPosition) {
                    targetPosition = currentPosition + distance;
                } else {
                    targetPosition = currentPosition - distance;
                }
            }
            // start by calculating a triangular motion with unlimited
            float requiredSpeed = (2 * distance) / timeSeconds;
            // constrain it to legal maximums
            requiredSpeed = constrain(requiredSpeed, 100.0f, maxSpeed);
            // calculate what proportion of the move needs to be at max speed, if any.
            float vt = requiredSpeed * timeSeconds;
            float proportion = max(-((2 * distance - 2 * vt)/vt),0.01f);
            // calculate acceleration such that triangle or trapezoid is created depending on needs
            float requiredAccel = requiredSpeed / (timeSeconds * proportion / 2);
            // constrain just in case, but reducing the distance should mostly preven this.
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
