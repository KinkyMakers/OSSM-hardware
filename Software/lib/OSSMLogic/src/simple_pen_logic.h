#pragma once

#include <cmath>
#include <cstdint>

// Pure logic extracted from src/ossm/simple_penetration/simple_penetration.cpp.
// No hardware dependencies — testable on native platform.

namespace simple_pen_logic {

/// Calculate stepper speed in steps/s from a speed percentage.
/// simple_penetration.cpp lines 33-34
inline float calculateSpeed(float speedPercent, float maxSpeedMmPerSec,
                             float stepsPerMm) {
    return stepsPerMm * maxSpeedMmPerSec * speedPercent / 100.0f;
}

/// Calculate stepper acceleration in steps/s^2 from a speed percentage.
/// Acceleration scales quadratically with speed (speed^2).
/// simple_penetration.cpp lines 35-37
inline float calculateAcceleration(float speedPercent, float maxSpeedMmPerSec,
                                   float accelerationScaling,
                                   float stepsPerMm) {
    return stepsPerMm * maxSpeedMmPerSec * speedPercent * speedPercent /
           accelerationScaling;
}

/// Check if the speed knob is in the dead zone (effectively zero).
/// simple_penetration.cpp lines 39-40
inline bool isInDeadZone(float knobValue, float deadZonePercentage) {
    return knobValue < deadZonePercentage;
}

/// Check if the speed change is large enough to warrant a motor update.
/// simple_penetration.cpp lines 42-43
inline bool isSpeedChangeSignificant(float oldSpeed, float newSpeed,
                                     float deadZonePercentage) {
    return std::abs(newSpeed - oldSpeed) > 5 * deadZonePercentage;
}

/// Calculate the target position for the next stroke.
/// simple_penetration.cpp lines 77-82
inline int32_t calculateTarget(bool isForward, float strokePercent,
                                float measuredStrokeSteps) {
    if (isForward) {
        return -std::abs((strokePercent / 100.0f) * measuredStrokeSteps);
    }
    return 0;
}

/// Calculate distance traveled in one stroke, in meters.
/// simple_penetration.cpp lines 97-100
inline double calculateStrokeDistance(float strokePercent,
                                     float measuredStrokeSteps,
                                     float stepsPerMm) {
    return ((strokePercent / 100.0f) * measuredStrokeSteps / stepsPerMm) /
           1000.0;
}

}  // namespace simple_pen_logic
