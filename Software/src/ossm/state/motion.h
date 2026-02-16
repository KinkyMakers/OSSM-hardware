#ifndef OSSM_STATE_MOTION_H
#define OSSM_STATE_MOTION_H

#include <Arduino.h>

/**
 * Motion state - tracks target position/velocity for motion control
 */
struct MotionState {
    float targetPosition = 0;
    float targetVelocity = 0;
    uint16_t targetTime = 0;
};

extern MotionState motion;

// Helper function for setting motion target
inline void moveTo(float intensity, uint16_t inTime) {
    motion.targetPosition = constrain(intensity, 0.0f, 100.0f);
    motion.targetTime = inTime;
}

#endif  // OSSM_STATE_MOTION_H
