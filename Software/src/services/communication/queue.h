#ifndef OSSM_COMMUNICATION_QUEUE_H
#define OSSM_COMMUNICATION_QUEUE_H

#include <Arduino.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <queue>

struct PositionTime {
    uint8_t position;                               // 0 - 100
    uint16_t inTime;                                // in ms
    // Captured in the BLE callback, before the target reaches either queue or
    // planner, so end-to-end input age includes firmware processing delay.
    std::chrono::steady_clock::time_point receivedAt;
    uint32_t receivedSequence = 0;
};

struct TargetQueueRead {
    uint32_t droppedWaypoints = 0;
    uint32_t oldestAgeMilliseconds = 0;
    uint32_t selectedAgeMilliseconds = 0;
    uint32_t bufferedDurationBeforeMilliseconds = 0;
    uint32_t selectedSequence = 0;
};

extern std::queue<String> messageQueue;

constexpr size_t kTargetQueueDepth = 64;

// The streaming target queue is written by both BLE command paths and read by
// the high-priority Streaming task. These helpers keep all access synchronized
// and avoid allocation while motion is active.
void initializeTargetQueue();
bool enqueueTarget(const PositionTime &target);
bool dequeueTarget(PositionTime &target);
bool dequeueFreshTarget(PositionTime &target, uint32_t maximumAgeMilliseconds,
                        uint32_t maximumBufferedDurationMilliseconds,
                        TargetQueueRead &result);
void clearTargetQueue();
size_t targetQueueSize();
uint32_t targetQueueBufferedDurationMs();
uint32_t targetQueueOverflowCount();

#endif  // OSSM_COMMUNICATION_QUEUE_H
