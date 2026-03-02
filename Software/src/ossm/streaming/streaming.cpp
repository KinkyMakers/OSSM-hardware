#include "streaming.h"

#include <queue>
#include <chrono>
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

namespace streaming {

static void startStreamingTask(void *pvParameters) {
    auto isInCorrectState = []() {
        return stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.preflight"_s) ||
               stateMachine->is("streaming.idle"_s);
    };
    // Reset to home position
    stepper->moveTo(0, true);
    vTaskDelay(pdMS_TO_TICKS(1000));

    auto best = std::chrono::steady_clock::now();
    PositionTime lastPositionTime;
    
    // Reset the queue to clear any existing commands
    targetQueue = {};
    
    // Motion state
    int16_t currentPosition = 0;
    int16_t targetPosition = 0;

    uint16_t maxSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
    uint32_t maxAccel = Config::Driver::maxAcceleration * (1_mm);

    // Set initial max speed and acceleration
    stepper->setSpeedInHz(maxSpeed);
    stepper->setAcceleration(maxAccel);

    while (isInCorrectState()) {
        uint16_t currentBuffer = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - best).count();
        // Wait for new command from BLE
        if (targetQueue.empty()){
            vTaskDelay(1);
            continue;
        }
        // Get next move
        PositionTime targetPositionTime = targetQueue.front();
        //Wait for previous command to finish if it isn't moving in the same direction.
        int16_t distance = targetPositionTime.position - lastPositionTime.position;
        targetPositionTime.direction = distance/abs(distance);
        bool sameDirection = lastPositionTime.direction == targetPositionTime.direction;
        if (!sameDirection && stepper->isRunning()){
            vTaskDelay(1);
            continue;
        }
        targetQueue.pop();
        
        float timeSeconds = targetPositionTime.inTime / 1000.0f;
        
        // settime is when the message was received. If we trust the source we can reduce perceived lag by creating a buffer.
        if (targetPositionTime.setTime){
            int16_t mincomp =  min(int(settings.buffer * 2),int(lastPositionTime.inTime));
            int16_t offset = mincomp - currentBuffer;
            int16_t lag = std::chrono::duration_cast<std::chrono::milliseconds>(targetPositionTime.setTime.value() - best).count();
            if (lag < 0 || lag > mincomp * 10){
                best = targetPositionTime.setTime.value();
                lag = 0;
                offset = 0;
            }
            best += std::chrono::milliseconds(targetPositionTime.inTime);
            ESP_LOGI("Streaming", "Lag: %d, Buffer: %d/%d", lag, currentBuffer, mincomp);
            if (offset < 0){
                // Shorten time up to 1/4 of the total time.
                offset = max(int16_t(targetPositionTime.inTime/-4), offset);
            }
            timeSeconds += offset/1000.0f;
        } else {
            best = std::chrono::steady_clock::now();
        }
        lastPositionTime = targetPositionTime;
        //Grab the minimal value between depth and stroke, use as max stroke length.
        int32_t maxStroke = abs(((float)min(settings.stroke,settings.depth) / 100.0) * calibration.measuredStrokeSteps);
        //Set 100% at max depth and constrain speeds based on user inputs.
        int32_t depth = (calibration.measuredStrokeSteps - maxStroke) * (settings.depth/100.0);
        uint32_t speedLimit = maxSpeed * (settings.speed/100.0);
        uint32_t accelLimit = maxAccel * (settings.sensation/100.0);
        // skip movement if speeds are 0
        if (speedLimit > 0 && accelLimit > 0){
            targetPosition = -(1-(static_cast<float>(targetPositionTime.position) / 100.0f)) * maxStroke - depth;
            currentPosition = stepper->getCurrentPosition();
            // Calculate distance to travel (in steps)
            distance = abs(targetPosition - currentPosition);
            if (timeSeconds > 0.01f && distance > 1.0f) {
                //Find max distance possible to travel given available time, max acceleration, and max speed.
                int32_t maxDistance = accelLimit * pow(timeSeconds/2,2);
                //This technically optimistic... the speed limit assumes perfect acceleration
                maxDistance = min(maxDistance, int32_t(speedLimit * timeSeconds));
                //if the distance asked for is greater then the maximum possible, reduce the ask.
                //Is it what they asked for? No. Will they notice at these speeds? Hopefully not.
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
                uint32_t requiredSpeed = (2 * distance) / timeSeconds;
                // constrain it to legal maximums
                requiredSpeed = constrain(requiredSpeed, 100.0f, speedLimit);
                // calculate what proportion of the move needs to be at max speed, if any.
                float vt = requiredSpeed * timeSeconds;
                float proportion = max(-((2 * distance - 2 * vt)/vt),0.01f);
                // calculate acceleration such that triangle or trapezoid is created depending on needs
                uint32_t requiredAccel = requiredSpeed / (timeSeconds * proportion / 2);
                // constrain just in case, but reducing the distance should mostly preven this.
                requiredAccel = constrain(requiredAccel, 100.0f, accelLimit);
                if (stepper->isRunning()){
                    requiredAccel = max(stepper->getAcceleration(),requiredAccel);
                }
                stepper->setAcceleration(requiredAccel);
                stepper->setSpeedInHz(requiredSpeed);
                stepper->moveTo(targetPosition, false);

                ESP_LOGI("Streaming", "P(%d): %d -> %d = %d, T: %.3f, S: %d, A: %d, Q: %d",
                        targetPositionTime.position, currentPosition, targetPosition, distance, 
                        timeSeconds, requiredSpeed, requiredAccel, targetQueue.size());
            }
        } else {
            ESP_LOGI("Streaming", "Spped or accel too slow, skipping moves");
        }
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
