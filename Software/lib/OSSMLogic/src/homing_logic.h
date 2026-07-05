#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

// Pure logic extracted from src/ossm/homing/homing.cpp.
// No hardware dependencies — testable on native platform.

namespace homing_logic {

/// Check if measured current exceeds the sensorless homing threshold.
/// homing.cpp lines 98-99
inline bool isCurrentOverLimit(float currentReading, float offset,
                               float threshold) {
    return (currentReading - offset) > threshold;
}

/// Calculate the measured stroke from the stepper's current position,
/// clamped to the maximum physical stroke.
/// homing.cpp lines 116-118
inline float calculateMeasuredStroke(int32_t currentPosition, 
                                     int32_t currentMeasuredSteps,
                                     float maxStrokeSteps) {
    return std::min(static_cast<float>(std::max(std::abs(currentPosition),
                                currentMeasuredSteps)),maxStrokeSteps);
}

/// Check if homing has exceeded the timeout.
/// homing.cpp line 81
inline bool isHomingTimedOut(uint32_t elapsedMs, uint32_t timeoutMs) {
    return elapsedMs > timeoutMs;
}

/// Check if the measured stroke is too short for safe operation.
/// homing.cpp lines 147-148 (homing::isStrokeTooShort)
inline bool isStrokeTooShortLogic(float measuredStrokeSteps,
                                  float minStrokeLengthMm) {
    return measuredStrokeSteps <= minStrokeLengthMm;
}

}  // namespace homing_logic
