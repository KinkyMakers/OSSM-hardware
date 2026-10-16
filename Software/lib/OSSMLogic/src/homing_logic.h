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
                                     float maxStrokeSteps) {
    return std::min(static_cast<float>(std::abs(currentPosition)),
                    maxStrokeSteps);
}

/// Calculate where to move after homing completes.
/// afterHomingPosition is a fraction of the measured stroke, clamped to 0..1
/// so a bad stored value can never drive past the far end.
/// homing.cpp lines 123-127
inline int32_t calculatePostHomingPosition(int16_t sign,
                                           float measuredStrokeSteps,
                                           float afterHomingPosition) {
    const float fraction =
        std::isfinite(afterHomingPosition)
            ? std::min(std::max(afterHomingPosition, 0.0f), 1.0f)
            : 0.0f;
    int32_t goToPosition = -sign * measuredStrokeSteps;
    if (goToPosition < 0) {
        goToPosition = goToPosition * fraction;
    }
    return goToPosition;
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
