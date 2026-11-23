#include "streaming.h"

#include <atomic>
#include <cmath>
#include <queue>
#include <chrono>
#include <mutex>
#include "queued_moves.h"
#include "streaming_logic.h"
#include "constants/Config.h"
#include "ossm/state/calibration.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/communication/nimble.h"
#include "services/communication/queue.h"
#include "services/stepper.h"
#include "services/tasks.h"

namespace sml = boost::sml;
using namespace sml;

namespace streaming {

// Queued moves ("q:" command). Written by the BLE task, consumed by the
// streaming task, so every access to the buffer holds moveBufferMutex.
static std::mutex moveBufferMutex;
static queued_moves::MoveBuffer moveBuffer;
// True while the streaming task runs; "q:" is rejected otherwise.
static std::atomic<bool> acceptingMoves{false};
// Set by "q:" with no moves; the streaming task stops the motor.
static std::atomic<bool> stopRequested{false};
// The range q:/r: position bytes are decoded in (set by r:). Moves are
// decoded as they arrive, so a later range never changes queued ones.
// Guarded by moveBufferMutex.
static queued_moves::Range moveRange;
// The queued move running now: a move ends when its time is up, whether or
// not it reached its position. The next one starts at that end time so
// timing doesn't drift, or on arrival if the buffer had run dry. Guarded by
// moveBufferMutex, since the low-water byte reads it from the BLE task.
static bool activeMove = false;
static uint32_t activeMoveEndMs = 0;
// ms of queued moves the device aims to keep (set:lowwater:n).
static std::atomic<uint32_t> lowWaterMs{queued_moves::kDefaultLowWaterMs};
// A queued move completed with the buffer below the low-water mark; the BLE
// loop sends a status for it (rate limited) and clears this.
static std::atomic<bool> lowWaterEvent{false};

static void clearQueuedMoves() {
    std::lock_guard<std::mutex> lock(moveBufferMutex);
    moveBuffer.clear();
    activeMove = false;
}

static void resetActiveMove() {
    std::lock_guard<std::mutex> lock(moveBufferMutex);
    activeMove = false;
}

// Call with moveBufferMutex held.
static uint8_t lowWaterSlotsLocked(uint32_t nowMs) {
    int32_t remainingMs =
        activeMove ? static_cast<int32_t>(activeMoveEndMs - nowMs) : 0;
    if (remainingMs < 0) {
        remainingMs = 0;
    }
    return queued_moves::lowWaterSlots(
        static_cast<uint32_t>(remainingMs) + moveBuffer.totalMs(), lowWaterMs,
        moveBuffer.freeSlots());
}

enum class QueueStep { Running, Started, Idle };

// Advance the queued moves at nowMs: keep the running move, or when its time
// is up start the next (filling `move` and `remainingMs`), or go idle.
static QueueStep advanceQueuedMoves(uint32_t nowMs, queued_moves::Move &move,
                                    int32_t &remainingMs) {
    std::lock_guard<std::mutex> lock(moveBufferMutex);
    if (activeMove && static_cast<int32_t>(nowMs - activeMoveEndMs) < 0) {
        return QueueStep::Running;
    }
    const bool completed = activeMove;
    QueueStep step = QueueStep::Idle;
    if (moveBuffer.pop(move)) {
        const uint32_t startMs = activeMove ? activeMoveEndMs : nowMs;
        activeMoveEndMs = startMs + move.timeMs;
        activeMove = true;
        remainingMs = static_cast<int32_t>(activeMoveEndMs - nowMs);
        step = QueueStep::Started;
    } else {
        activeMove = false;
    }
    if (completed && lowWaterSlotsLocked(nowMs) != 0) {
        lowWaterEvent = true;
    }
    return step;
}

void setLowWaterMs(uint32_t ms) {
    lowWaterMs = ms;
    ESP_LOGI("Streaming", "Low water mark: %u ms", static_cast<unsigned>(ms));
}

uint8_t lowWaterSlots() {
    // Outside streaming mode q: is refused, so there's nothing to ask for.
    if (!acceptingMoves) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(moveBufferMutex);
    return lowWaterSlotsLocked(millis());
}

bool takeLowWaterEvent() { return lowWaterEvent.exchange(false); }

bool handleQueueCommand(const uint8_t *data, size_t length) {
    const auto parsed = queued_moves::parseQueueCommand(data, length);
    if (!parsed) {
        ESP_LOGW("Streaming", "Malformed q: command (%u bytes)",
                 static_cast<unsigned>(length));
        return false;
    }
    if (!acceptingMoves) {
        ESP_LOGW("Streaming", "q: refused: not in streaming mode");
        return false;
    }

    if (parsed->count == 0) {
        clearQueuedMoves();
        stopRequested = true;
        ESP_LOGI("Streaming", "q: cleared buffer, stopping");
        return true;
    }

    std::lock_guard<std::mutex> lock(moveBufferMutex);
    if (!moveBuffer.pushAll(parsed->payload, parsed->count, moveRange)) {
        ESP_LOGW("Streaming", "q: refused: %u moves, %u free slots",
                 static_cast<unsigned>(parsed->count),
                 static_cast<unsigned>(moveBuffer.freeSlots()));
        return false;
    }
    return true;
}

bool handleRangeCommand(const uint8_t *data, size_t length) {
    const auto parsed = queued_moves::parseRangeCommand(data, length);
    if (!parsed) {
        ESP_LOGW("Streaming", "Malformed r: command (%u bytes)",
                 static_cast<unsigned>(length));
        return false;
    }
    if (!acceptingMoves) {
        ESP_LOGW("Streaming", "r: refused: not in streaming mode");
        return false;
    }

    std::lock_guard<std::mutex> lock(moveBufferMutex);
    // The range only takes effect along with its moves.
    if (!moveBuffer.pushAll(parsed->payload, parsed->count, parsed->range)) {
        ESP_LOGW("Streaming", "r: refused: %u moves, %u free slots",
                 static_cast<unsigned>(parsed->count),
                 static_cast<unsigned>(moveBuffer.freeSlots()));
        return false;
    }
    moveRange = parsed->range;
    return true;
}

// Start one queued move that must finish within remainingMs.
static void startQueuedMove(const queued_moves::Move &move,
                            int32_t remainingMs, uint32_t maxSpeed,
                            uint32_t maxAccel) {
    const uint32_t speedLimit = maxSpeed * (settings.speed / 100.0);
    const uint32_t accelLimit = maxAccel * (settings.sensation / 100.0);
    if (speedLimit == 0 || accelLimit == 0) {
        ESP_LOGD("Streaming", "Speed or accel is 0, skipping queued move");
        return;
    }
    // Too little time left to plan a sensible move; let it lapse.
    if (remainingMs < 10) {
        return;
    }

    const int32_t maxStroke = streaming_logic::calculateMaxStroke(
        settings.stroke, settings.depth, calibration.measuredStrokeSteps);
    const int32_t depth = streaming_logic::calculateDepthOffset(
        calibration.measuredStrokeSteps, maxStroke, settings.depth);
    const int32_t targetPosition = static_cast<int32_t>(std::lround(
        queued_moves::scaleQueuedPosition(move.position, maxStroke, depth)));
    const int32_t currentPosition = stepper->getCurrentPosition();
    if (abs(targetPosition - currentPosition) <= 1) {
        return;
    }

    const auto profile = streaming_logic::planMotion(
        currentPosition, targetPosition, remainingMs / 1000.0f, speedLimit,
        accelLimit, static_cast<int32_t>(1_mm));
    stepper->setAcceleration(profile.acceleration);
    stepper->setSpeedInHz(profile.speed);
    stepper->moveTo(profile.targetPosition, false);

    ESP_LOGD("Streaming", "Q(%u): %d -> %d, T: %d, S: %u, A: %u",
             move.position, currentPosition, profile.targetPosition,
             remainingMs, profile.speed, profile.acceleration);
}

static void startStreamingTask(void *pvParameters) {
    // Own the shared stepper config at mode entry (see simple_penetration.cpp:
    // StrokeEngine leaves the shared DIR polarity inverted and the counter in
    // its own frame; this mode's targets are native-frame absolutes).
    stepperTranslateFrame(StepperFrame::Native);
    stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
    stepper->enableOutputs();

    auto isInCorrectState = []() {
        return stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.preflight"_s) ||
               stateMachine->is("streaming.idle"_s);
    };

    auto best = std::chrono::steady_clock::now();
    PositionTime lastPositionTime;
    
    // Reset the queue to clear any existing commands
    targetQueue = {};
    clearQueuedMoves();
    {
        std::lock_guard<std::mutex> lock(moveBufferMutex);
        moveRange = {};
    }
    stopRequested = false;
    lowWaterEvent = false;
    acceptingMoves = true;

    // Motion state
    int16_t currentPosition = 0;
    int16_t targetPosition = 0;

    uint16_t maxSpeed = Config::Driver::maxSpeedMmPerSecond * (1_mm);
    uint32_t maxAccel = Config::Driver::maxAcceleration * (1_mm);

    // Set initial max speed and acceleration
    stepper->setSpeedInHz(maxSpeed);
    stepper->setAcceleration(maxAccel);

    while (isInCorrectState()) {
        // "q:" with no moves: drop everything pending and stop.
        if (stopRequested.exchange(false)) {
            targetQueue = {};
            resetActiveMove();
                    lastPositionTime = {};
            stepper->stopMove();
            vTaskDelay(1);
            continue;
        }

        // Queued moves take priority over stream: targets.
        queued_moves::Move queuedMove;
        int32_t remainingMs = 0;
        const QueueStep step =
            advanceQueuedMoves(millis(), queuedMove, remainingMs);
        if (step == QueueStep::Started) {
            startQueuedMove(queuedMove, remainingMs, maxSpeed, maxAccel);
        }
        if (step != QueueStep::Idle) {
            vTaskDelay(1);
            continue;
        }

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
        if (USE_LATENCY_COMPENSATION){
            int16_t mincomp =  min(int(settings.buffer * 2),int(lastPositionTime.inTime));
            int16_t offset = mincomp - currentBuffer;
            int16_t lag = std::chrono::duration_cast<std::chrono::milliseconds>(targetPositionTime.setTime - best).count();
            if (lag < 0 || lag > mincomp * 10){
                best = targetPositionTime.setTime;
                lag = 0;
                offset = 0;
            }
            best += std::chrono::milliseconds(targetPositionTime.inTime);
            ESP_LOGD("Streaming", "Lagency Compensation - Lag: %d, Buffer: %d/%d", lag, currentBuffer, mincomp);
            if (offset < 0){
                // Shorten time up to 1/4 of the total time.
                offset = max(int16_t(targetPositionTime.inTime/-4), offset);
            }
            timeSeconds += offset/1000.0f;
        } else {
            best = std::chrono::steady_clock::now();
        }
        lastPositionTime = targetPositionTime;
        int32_t maxStroke = streaming_logic::calculateMaxStroke(
            settings.stroke, settings.depth,
            calibration.measuredStrokeSteps);
        int32_t depth = streaming_logic::calculateDepthOffset(
            calibration.measuredStrokeSteps, maxStroke, settings.depth);
        uint32_t speedLimit = maxSpeed * (settings.speed/100.0);
        uint32_t accelLimit = maxAccel * (settings.sensation/100.0);
        // skip movement if speeds are 0
        if (speedLimit > 0 && accelLimit > 0){
            targetPosition = streaming_logic::scaleStreamPosition(
                targetPositionTime.position, maxStroke, depth);
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

    acceptingMoves = false;
    clearQueuedMoves();
    vTaskDelete(nullptr);
}

void startStreaming() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 1, nullptr,
                            Tasks::operationTaskCore);
}

}  // namespace simple_penetration
