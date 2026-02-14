#ifndef OSSM_COMMUNICATION_QUEUE_H
#define OSSM_COMMUNICATION_QUEUE_H

#include <Arduino.h>
#include <chrono>
#include <queue>

struct PositionTime {
    uint8_t position; // 0 - 100
    uint16_t inTime;     // in ms
    std::optional<std::chrono::steady_clock::time_point> setTime; //received timestamp
};

extern std::queue<String> messageQueue;
extern std::queue<PositionTime> targetQueue;

#endif  // OSSM_COMMUNICATION_QUEUE_H
