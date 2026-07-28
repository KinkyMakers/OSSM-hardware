#include "queue.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

std::queue<String> messageQueue = {};

namespace {

    StaticQueue_t targetQueueControl;
    uint8_t targetQueueStorage[kTargetQueueDepth * sizeof(PositionTime)];
    QueueHandle_t targetQueue = nullptr;
    portMUX_TYPE targetQueueStatsMux = portMUX_INITIALIZER_UNLOCKED;
    uint32_t bufferedDurationMs = 0;
    uint32_t overflowCount = 0;

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
    const BaseType_t result = xQueueSend(targetQueue, &target, 0);
    if (result != pdTRUE) {
        ++overflowCount;
        taskEXIT_CRITICAL(&targetQueueStatsMux);
        return false;
    }

    bufferedDurationMs += target.inTime;
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
    if (bufferedDurationMs >= target.inTime) {
        bufferedDurationMs -= target.inTime;
    } else {
        bufferedDurationMs = 0;
    }
    taskEXIT_CRITICAL(&targetQueueStatsMux);
    return true;
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
