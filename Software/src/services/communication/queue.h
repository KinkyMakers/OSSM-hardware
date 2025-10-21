#ifndef OSSM_COMMUNICATION_QUEUE_H
#define OSSM_COMMUNICATION_QUEUE_H

#include <Arduino.h>

#include <queue>

struct PositionTime {
    // 0 - 180
    uint8_t position;
    // in ms
    uint16_t inTime;
};

extern std::queue<String> messageQueue;
extern PositionTime lastPositionTime;
extern PositionTime targetPositionTime;

#endif  // OSSM_COMMUNICATION_QUEUE_H
