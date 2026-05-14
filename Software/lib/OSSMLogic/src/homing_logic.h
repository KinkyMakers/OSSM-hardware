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
/// homing.cpp lines 123-127
///
/// Path X (invertDirection=true) sign convention:
///   homing.forward  (sign=+1, just zeroed at extended hard stop)
///       → goToPosition = -1 * M = -M (drive across full range to retracted side,
///                                     where homing.backward will detect the stop).
///   homing.backward (sign=-1, just zeroed at retracted hard stop = counter 0)
///       → goToPosition = +1 * M = +M, then * afterHomingPosition (default 0)
///         lands the rod at counter=0 (stay at retracted) or some fraction of
///         the working range if afterHomingPosition is set non-zero.
inline int32_t calculatePostHomingPosition(int16_t sign,
                                           float measuredStrokeSteps,
                                           float afterHomingPosition) {
    int32_t goToPosition = -sign * measuredStrokeSteps;
    if (goToPosition > 0) {
        goToPosition = goToPosition * afterHomingPosition;
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
