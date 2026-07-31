#include "queue.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <algorithm>
#include <chrono>

#include "stream_backlog_policy.h"

std::queue<String> messageQueue = {};

namespace {

    StaticQueue_t targetQueueControl;
    uint8_t targetQueueStorage[kTargetQueueDepth * sizeof(PositionTime)];
    QueueHandle_t targetQueue = nullptr;
    portMUX_TYPE targetQueueStatsMux = portMUX_INITIALIZER_UNLOCKED;
    uint32_t bufferedDurationMs = 0;
    uint32_t overflowCount = 0;
    uint32_t receivedSequence = 0;

    void subtractBufferedDuration(const PositionTime &target) {
        if (bufferedDurationMs >= target.inTime) {
            bufferedDurationMs -= target.inTime;
        } else {
            bufferedDurationMs = 0;
        }
    }

    uint32_t waypointAgeMilliseconds(
        const PositionTime &target,
        std::chrono::steady_clock::time_point now) {
        const auto elapsed = now - target.receivedAt;
        if (elapsed <= std::chrono::steady_clock::duration::zero()) return 0;
        const auto milliseconds =
            std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count();
        return static_cast<uint32_t>(std::min<int64_t>(
            milliseconds, static_cast<int64_t>(UINT32_MAX)));
    }

}  // namespace

void initializeTargetQueue() {
    if (targetQueue != nullptr) return;

    taskENTER_CRITICAL(&targetQueueStatsMux);
    if (targetQueue == nullptr) {
        targetQueue =
            xQueueCreateStatic(kTargetQueueDepth, sizeof(PositionTime),
                               targetQueueStorage, &targetQueueControl);
    }
    taskEXIT_CRITICAL(&targetQueueStatsMux);
}

bool enqueueTarget(const PositionTime &target) {
    initializeTargetQueue();
    taskENTER_CRITICAL(&targetQueueStatsMux);
    PositionTime received = target;
    received.receivedSequence = ++receivedSequence;
    BaseType_t result = xQueueSend(targetQueue, &received, 0);
    if (result != pdTRUE) {
        ++overflowCount;
        PositionTime obsolete{};
        if (xQueueReceive(targetQueue, &obsolete, 0) == pdTRUE) {
            subtractBufferedDuration(obsolete);
            result = xQueueSend(targetQueue, &received, 0);
        }
        if (result != pdTRUE) {
            taskEXIT_CRITICAL(&targetQueueStatsMux);
            return false;
        }
    }

    bufferedDurationMs += received.inTime;
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return true;
}

bool dequeueTarget(PositionTime &target) {
    initializeTargetQueue();
    taskENTER_CRITICAL(&targetQueueStatsMux);
    if (xQueueReceive(targetQueue, &target, 0) != pdTRUE) {
        taskEXIT_CRITICAL(&targetQueueStatsMux);
        return false;
    }
    subtractBufferedDuration(target);
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return true;
}

bool dequeueFreshTarget(PositionTime &target, uint32_t maximumAgeMilliseconds,
                        uint32_t maximumBufferedDurationMilliseconds,
                        TargetQueueRead &result) {
    initializeTargetQueue();
    result = {};
    const auto now = std::chrono::steady_clock::now();

    taskENTER_CRITICAL(&targetQueueStatsMux);
    result.bufferedDurationBeforeMilliseconds = bufferedDurationMs;
    while (uxQueueMessagesWaiting(targetQueue) != 0) {
        PositionTime oldest{};
        if (xQueuePeek(targetQueue, &oldest, 0) != pdTRUE) break;
        const uint32_t age = waypointAgeMilliseconds(oldest, now);
        if (result.droppedWaypoints == 0)
            result.oldestAgeMilliseconds = age;

        const size_t queuedWaypoints = uxQueueMessagesWaiting(targetQueue);
        if (stream_backlog_policy::shouldDiscardOldest(
                age, bufferedDurationMs, queuedWaypoints,
                maximumAgeMilliseconds,
                maximumBufferedDurationMilliseconds)) {
            if (xQueueReceive(targetQueue, &oldest, 0) != pdTRUE) break;
            subtractBufferedDuration(oldest);
            ++result.droppedWaypoints;
            continue;
        }

        if (xQueueReceive(targetQueue, &target, 0) != pdTRUE) break;
        subtractBufferedDuration(target);
        result.selectedAgeMilliseconds = age;
        result.selectedSequence = target.receivedSequence;
        taskEXIT_CRITICAL(&targetQueueStatsMux);
        return true;
    }
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return false;
}

void clearTargetQueue() {
    initializeTargetQueue();
    taskENTER_CRITICAL(&targetQueueStatsMux);
    xQueueReset(targetQueue);
    bufferedDurationMs = 0;
    taskEXIT_CRITICAL(&targetQueueStatsMux);
}

size_t targetQueueSize() {
    initializeTargetQueue();
    return uxQueueMessagesWaiting(targetQueue);
}

uint32_t targetQueueBufferedDurationMs() {
    taskENTER_CRITICAL(&targetQueueStatsMux);
    const uint32_t result = bufferedDurationMs;
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return result;
}

uint32_t targetQueueOverflowCount() {
    taskENTER_CRITICAL(&targetQueueStatsMux);
    const uint32_t result = overflowCount;
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return result;
}
