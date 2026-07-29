#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

// Pure logic extracted from src/ossm/homing/homing.cpp.
// No hardware dependencies — testable on native platform.

namespace homing_logic {

enum class ProbeDirection : int8_t {
    Unsafe = 0,
    Negative = -1,
    Positive = 1,
};

struct WiggleTargets {
    int32_t negative = 0;
    int32_t positive = 0;
    uint32_t totalTravel = 0;
};

inline WiggleTargets calculateWiggleTargets(int32_t origin,
                                            uint32_t distance) {
    return WiggleTargets{
        origin - static_cast<int32_t>(distance),
        origin + static_cast<int32_t>(distance),
        distance * 3,
    };
}

inline bool hasProbeSignal(float negativeLoad, float positiveLoad,
                           float minimumSignal) {
    return std::isfinite(negativeLoad) && std::isfinite(positiveLoad) &&
           minimumSignal >= 0 &&
           std::max(std::abs(negativeLoad), std::abs(positiveLoad)) >=
               minimumSignal;
}

/// Check if measured current exceeds the sensorless homing threshold.
/// homing.cpp lines 98-99
inline bool isCurrentOverLimit(float currentReading, float offset,
                               float threshold) {
    return (currentReading - offset) > threshold;
}

/// Select the direction that is least likely to be pushing into a hard stop.
/// Probe loads are magnitudes relative to the motor-off sensor offset.
inline ProbeDirection chooseProbeEscapeDirection(
    float negativeLoad, float positiveLoad, float hardLimit,
    float minimumSignal, float tieMargin) {
    if (!std::isfinite(negativeLoad) || !std::isfinite(positiveLoad) ||
        hardLimit <= 0 || minimumSignal < 0 || tieMargin < 0) {
        return ProbeDirection::Unsafe;
    }

    negativeLoad = std::abs(negativeLoad);
    positiveLoad = std::abs(positiveLoad);
    if (std::max(negativeLoad, positiveLoad) < minimumSignal) {
        return ProbeDirection::Unsafe;
    }

    const bool negativeBlocked = negativeLoad >= hardLimit;
    const bool positiveBlocked = positiveLoad >= hardLimit;
    if (negativeBlocked && positiveBlocked) return ProbeDirection::Unsafe;
    if (negativeBlocked) return ProbeDirection::Positive;
    if (positiveBlocked) return ProbeDirection::Negative;

    if (std::abs(negativeLoad - positiveLoad) <= tieMargin) {
        return ProbeDirection::Unsafe;
    }
    return negativeLoad < positiveLoad ? ProbeDirection::Negative
                                       : ProbeDirection::Positive;
}

/// Resolve two longer current-limited wiggles. A direction that reached the
/// adaptive current limit is treated as blocked even when its partial-run
/// average is diluted by low-current startup samples.
inline ProbeDirection chooseWiggleEscapeDirection(
    float negativeLoad, float positiveLoad, bool negativeBlocked,
    bool positiveBlocked, float minimumSignal, float tieMargin,
    ProbeDirection tieFallback = ProbeDirection::Unsafe) {
    if (negativeBlocked && positiveBlocked) return ProbeDirection::Unsafe;
    if (negativeBlocked) return ProbeDirection::Positive;
    if (positiveBlocked) return ProbeDirection::Negative;
    const ProbeDirection measuredDirection = chooseProbeEscapeDirection(
        negativeLoad, positiveLoad, std::numeric_limits<float>::max(),
        minimumSignal, tieMargin);
    if (measuredDirection != ProbeDirection::Unsafe) {
        return measuredDirection;
    }
    if (!hasProbeSignal(negativeLoad, positiveLoad, minimumSignal)) {
        return ProbeDirection::Unsafe;
    }
    return tieFallback;
}

/// Derive a contact threshold from the lower-current (freer) probe while
/// retaining the configured absolute ceiling.
inline float adaptiveCurrentLimit(float negativeLoad, float positiveLoad,
                                  float hardLimit, float requiredRise) {
    const float freeLoad =
        std::min(std::abs(negativeLoad), std::abs(positiveLoad));
    const float loadedDirection =
        std::max(std::abs(negativeLoad), std::abs(positiveLoad));
    const float midpoint = (freeLoad + loadedDirection) * 0.5f;
    return std::min(hardLimit,
                    std::max(freeLoad + requiredRise, midpoint));
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
inline int32_t calculatePostHomingPosition(int16_t sign,
                                           float measuredStrokeSteps,
                                           float afterHomingPosition) {
    int32_t goToPosition = -sign * measuredStrokeSteps;
    if (goToPosition < 0) {
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
