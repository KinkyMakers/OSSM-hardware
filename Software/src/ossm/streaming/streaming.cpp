#include "streaming.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <queue>
#include <chrono>
#include <mutex>
#include "queued_moves.h"
#include "streaming_logic.h"
#include "trajectory.h"
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

// How many buffered moves to look past the running one when shaping the path.
// The trajectory also holds the running move's start, its end and the
// keyframe before it.
static constexpr size_t kLookaheadMoves = trajectory::kMaxKeyframes - 3;

// What advanceQueuedMoves saw, taken under one lock.
struct QueueSnapshot {
    QueueStep step = QueueStep::Idle;
    queued_moves::Move started{};  // the move that began (step == Started)
    bool continued = false;        // it began where the previous one ended
    uint32_t startMs = 0;          // its start and end time (millis)
    uint32_t endMs = 0;
    queued_moves::Move upcoming[kLookaheadMoves];  // the next buffered moves
    size_t upcomingCount = 0;
};

// Advance the queued moves at nowMs: keep the running move, or when its time
// is up start the next, or go idle. Also copies out the moves queued after
// the running one, so the path can be shaped through them.
static void advanceQueuedMoves(uint32_t nowMs, QueueSnapshot &snap) {
    std::lock_guard<std::mutex> lock(moveBufferMutex);
    snap.step = QueueStep::Idle;
    if (activeMove && static_cast<int32_t>(nowMs - activeMoveEndMs) < 0) {
        snap.step = QueueStep::Running;
    } else {
        const bool completed = activeMove;
        if (moveBuffer.pop(snap.started)) {
            snap.continued = activeMove;
            snap.startMs = activeMove ? activeMoveEndMs : nowMs;
            activeMoveEndMs = snap.startMs + snap.started.timeMs;
            snap.endMs = activeMoveEndMs;
            activeMove = true;
            snap.step = QueueStep::Started;
        } else {
            activeMove = false;
        }
        if (completed && lowWaterSlotsLocked(nowMs) != 0) {
            lowWaterEvent = true;
        }
    }
    snap.upcomingCount = 0;
    if (snap.step == QueueStep::Idle) {
        return;
    }
    while (snap.upcomingCount < kLookaheadMoves &&
           moveBuffer.peek(snap.upcomingCount,
                           snap.upcoming[snap.upcomingCount])) {
        ++snap.upcomingCount;
    }
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

// Queued moves are followed as one smooth path rather than started one at a
// time: each move given its own moveTo would stop at its point, stuttering
// every few ms. Instead, every tick the motor is aimed at a point a little
// ahead on a curve through the queued points (see trajectory.h), at the
// speed that reaches it on time. The aim point keeps moving ahead, so the
// motor only slows where the path itself turns around. Only the streaming
// task touches this state.

// Keyframes of the moves already begun: the one before the running move's
// start, its start, and its end.
static trajectory::Trajectory startedKeyframes;
// What was last sent to the motor, so unchanged ticks send nothing.
static int32_t followTarget = 0;
static uint32_t followSpeed = 0;
static uint32_t followAccel = 0;
static bool following = false;

static void resetFollowing() {
    startedKeyframes.clear();
    following = false;
}

// Lowest speed the motor is asked for while following, in steps/s.
// Low, so slow paths (below 5 mm/s) are followed rather than overrun and
// waited out.
static constexpr uint32_t kMinFollowSpeed = 10;

// Steer the motor along the queued moves' path at nowMs. Called every tick
// while queued moves run, and once more as they run out, to settle on the
// last point.
static void followQueuedMoves(const QueueSnapshot &snap, uint32_t nowMs,
                              uint32_t maxSpeed, uint32_t maxAccel) {
    const int32_t maxStroke = streaming_logic::calculateMaxStroke(
        settings.stroke, settings.depth, calibration.measuredStrokeSteps);
    const int32_t depth = streaming_logic::calculateDepthOffset(
        calibration.measuredStrokeSteps, maxStroke, settings.depth);
    auto scale = [&](uint16_t position) {
        return queued_moves::scaleQueuedPosition(position, maxStroke, depth);
    };

    if (snap.step == QueueStep::Started) {
        // After a gap, the path starts from wherever the carriage is now.
        if (!snap.continued || startedKeyframes.empty()) {
            startedKeyframes.clear();
            startedKeyframes.push(
                snap.startMs,
                static_cast<float>(stepper->getCurrentPosition()));
        }
        startedKeyframes.push(snap.endMs, scale(snap.started.position));
        startedKeyframes.keepLast(3);
        ESP_LOGD("Streaming", "Q(%d): T: %u, ahead: %u",
                 snap.started.position,
                 static_cast<unsigned>(snap.endMs - snap.startMs),
                 static_cast<unsigned>(snap.upcomingCount));
    }
    if (startedKeyframes.empty()) {
        return;
    }

    const uint32_t speedLimit = maxSpeed * (settings.speed / 100.0);
    const uint32_t accelLimit = maxAccel * (settings.sensation / 100.0);
    if (speedLimit == 0 || accelLimit == 0) {
        ESP_LOGD("Streaming", "Speed or accel is 0, not following queued moves");
        return;
    }

    trajectory::Trajectory path = startedKeyframes;
    uint32_t timeMs = path.back().timeMs;
    for (size_t i = 0; i < snap.upcomingCount; ++i) {
        timeMs += snap.upcoming[i].timeMs;
        path.push(timeMs, scale(snap.upcoming[i].position));
    }

    const float currentSpeed =
        stepper->getCurrentSpeedInMilliHz() / 1000.0f;
    const uint32_t lookahead =
        trajectory::lookaheadMs(currentSpeed, static_cast<float>(accelLimit));
    // Keep the aim inside the stroke window, whatever the path does.
    const int32_t windowNear = -depth;
    const int32_t windowFar = -(maxStroke + depth);
    const float aim = std::clamp(path.positionAt(nowMs + lookahead),
                                 static_cast<float>(windowFar),
                                 static_cast<float>(windowNear));
    const int32_t target = static_cast<int32_t>(std::lround(aim));
    const int32_t currentPosition = stepper->getCurrentPosition();
    // From the unrounded aim, so the speed doesn't jump with step rounding.
    const uint32_t speed = trajectory::pursuitSpeed(
        aim - static_cast<float>(currentPosition), lookahead,
        std::min(kMinFollowSpeed, speedLimit), speedLimit);

    // Skip re-sending when nothing meaningful changed (speed within 2%).
    const uint32_t speedChange =
        speed > followSpeed ? speed - followSpeed : followSpeed - speed;
    if (following && target == followTarget && speedChange * 50 <= speed &&
        accelLimit == followAccel) {
        return;
    }
    if (target == currentPosition && !stepper->isRunning()) {
        followTarget = target;
        return;
    }
    stepper->setAcceleration(accelLimit);
    stepper->setSpeedInHz(speed);
    stepper->moveTo(target, false);
    followTarget = target;
    followSpeed = speed;
    followAccel = accelLimit;
    following = true;
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
    resetFollowing();
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
            resetFollowing();
            lastPositionTime = {};
            stepper->stopMove();
            vTaskDelay(1);
            continue;
        }

        // Queued moves take priority over stream: targets.
        const uint32_t nowMs = millis();
        QueueSnapshot snap;
        advanceQueuedMoves(nowMs, snap);
        if (snap.step != QueueStep::Idle) {
            followQueuedMoves(snap, nowMs, maxSpeed, maxAccel);
            vTaskDelay(1);
            continue;
        }
        if (!startedKeyframes.empty()) {
            // The queue ran out: settle on its last point, then hand over.
            followQueuedMoves(snap, nowMs, maxSpeed, maxAccel);
            resetFollowing();
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
