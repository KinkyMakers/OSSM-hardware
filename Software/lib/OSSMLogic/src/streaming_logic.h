#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

// Pure logic extracted from src/ossm/streaming/streaming.cpp.
// No hardware dependencies — testable on native platform.

namespace streaming_logic {

/// Compute max stroke in steps from user percentages and calibration.
/// streaming.cpp line 88
inline int32_t calculateMaxStroke(float strokePct, float depthPct,
                                  float measuredStrokeSteps) {
    return std::abs(
        (std::min(strokePct, depthPct) / 100.0f) * measuredStrokeSteps);
}

/// Compute depth offset in steps.
/// streaming.cpp line 90
inline int32_t calculateDepthOffset(float measuredStrokeSteps,
                                    int32_t maxStroke, float depthPct) {
    return (measuredStrokeSteps - maxStroke) * (depthPct / 100.0f);
}

/// Scale a BLE position percentage (0-100) into stepper target position.
/// streaming.cpp line 95
inline int32_t scaleStreamPosition(int posPercent, int32_t maxStroke,
                                   int32_t depth) {
    return -(1 - (static_cast<float>(posPercent) / 100.0f)) * maxStroke -
           depth;
}

/// Result of motion profile planning.
struct MotionProfile {
    uint32_t speed;
    uint32_t acceleration;
    int32_t distance;         // adjusted if physically impossible
    int32_t targetPosition;   // adjusted if distance was clamped
};

/// Plan a triangular/trapezoidal motion profile for a streaming move.
/// Extracted from streaming.cpp lines 99-125.
///
/// Given a distance to travel, available time, and machine limits,
/// computes the required speed and acceleration for a smooth move.
/// If the requested distance exceeds what's physically possible,
/// it clamps the distance and adjusts the target position.
inline MotionProfile planMotion(int32_t currentPosition,
                                int32_t targetPosition, float timeSeconds,
                                uint32_t maxSpeed, uint32_t maxAccel,
                                int32_t stepsPerMm = 1) {
    int32_t distance = std::abs(targetPosition - currentPosition);

    // Clamp distance to what's physically achievable
    int32_t maxDistance = maxAccel * std::pow(timeSeconds / 2, 2);
    maxDistance =
        std::min(maxDistance, static_cast<int32_t>(maxSpeed * timeSeconds));

    if (distance > maxDistance) {
        int32_t reducedDist = maxDistance - (2 * stepsPerMm);
        if (reducedDist < 0) reducedDist = 0;
        if (targetPosition > currentPosition) {
            targetPosition = currentPosition + reducedDist;
        } else {
            targetPosition = currentPosition - reducedDist;
        }
        distance = reducedDist;
    }

    // Triangular profile: speed = 2 * distance / time
    uint32_t requiredSpeed = (2 * distance) / timeSeconds;
    requiredSpeed = std::max(requiredSpeed, 100u);
    requiredSpeed = std::min(requiredSpeed, maxSpeed);

    // Proportion of move at max speed (trapezoid vs triangle)
    float vt = requiredSpeed * timeSeconds;
    float proportion = std::max(-((2.0f * distance - 2.0f * vt) / vt), 0.01f);

    // Acceleration to create the triangle/trapezoid
    uint32_t requiredAccel =
        requiredSpeed / (timeSeconds * proportion / 2.0f);
    requiredAccel = std::max(requiredAccel, 100u);
    requiredAccel = std::min(requiredAccel, maxAccel);

    return {requiredSpeed, requiredAccel, distance, targetPosition};
}

}  // namespace streaming_logic
