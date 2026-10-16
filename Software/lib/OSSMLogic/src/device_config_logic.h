#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

// Validation and derived values for settings stored in NVS.
// No hardware dependencies — testable on native platform.

namespace device_config_logic {

// FastAccelStepper allows 200 kHz on ESP32; this leaves headroom for
// interrupt latency while BLE, Wi-Fi and MQTT share the chip.
constexpr float kMaxStepRateHz = 100000.0f;

// Below this, 1 mm rounds to too few steps for the keepout and homing offset.
constexpr float kMinStepsPerMm = 2.0f;

// Homing reports "stroke too short" at or below this length.
constexpr float kMinStrokeLengthMm = 50.0f;

// Top acceleration the firmware converts to steps/s².
constexpr float kMaxAccelerationMmPerS2 = 50000.0f;

// Largest steps/mm whose max acceleration in steps/s² still fits in int32.
constexpr float kMaxStepsPerMm =
    static_cast<float>(std::numeric_limits<int32_t>::max()) /
    kMaxAccelerationMmPerS2;

/// Machine geometry. Applied once at boot; changes need a restart because
/// calibration and the stepper position counter are stored in steps.
struct MachineConfig {
    float stepsPerRev = 800.0f;
    float pulleyToothCount = 20.0f;
    float beltPitchMm = 2.0f;
    float maxRpm = 1500.0f;
    float maxStrokeMm = 500.0f;
};

enum class GeometryError {
    None,
    NotPositive,
    NotWholeNumber,
    StepsPerMmTooLow,
    StepRateTooHigh,
    StrokeTooShort,
    WouldOverflow,
    StorageFailed,
};

inline float stepsPerMm(const MachineConfig &config) {
    return config.stepsPerRev /
           (config.pulleyToothCount * config.beltPitchMm);
}

inline float maxStepRateHz(const MachineConfig &config) {
    return config.maxRpm * config.stepsPerRev / 60.0f;
}

inline float maxSpeedMmPerSecond(const MachineConfig &config) {
    return config.maxRpm / 60.0f * config.pulleyToothCount *
           config.beltPitchMm;
}

inline bool isWholeNumber(float value) {
    return std::isfinite(value) && std::floor(value) == value;
}

/// True when a step quantity can be stored in an int32_t.
inline bool fitsInt32(float steps) {
    return std::isfinite(steps) &&
           std::fabs(steps) <
               static_cast<float>(std::numeric_limits<int32_t>::max());
}

/// Every check is on derived step quantities, so the limits move with the
/// other settings. maxAccelerationMmPerS2 is the largest mm/s² value the
/// firmware converts to steps.
inline GeometryError validateGeometry(const MachineConfig &config,
                                      float maxAccelerationMmPerS2) {
    const float fields[] = {config.stepsPerRev, config.pulleyToothCount,
                            config.beltPitchMm, config.maxRpm,
                            config.maxStrokeMm};
    for (float field : fields) {
        if (!std::isfinite(field) || field <= 0.0f)
            return GeometryError::NotPositive;
    }
    if (!isWholeNumber(config.stepsPerRev) ||
        !isWholeNumber(config.pulleyToothCount))
        return GeometryError::NotWholeNumber;
    if (config.maxStrokeMm <= kMinStrokeLengthMm)
        return GeometryError::StrokeTooShort;

    const float perMm = stepsPerMm(config);
    if (!std::isfinite(perMm) || perMm < kMinStepsPerMm)
        return GeometryError::StepsPerMmTooLow;
    if (maxStepRateHz(config) > kMaxStepRateHz)
        return GeometryError::StepRateTooHigh;
    if (!fitsInt32(config.maxStrokeMm * perMm) ||
        !fitsInt32(maxAccelerationMmPerS2 * perMm))
        return GeometryError::WouldOverflow;
    return GeometryError::None;
}

inline const char *geometryErrorMessage(GeometryError error) {
    switch (error) {
        case GeometryError::None:
            return "ok";
        case GeometryError::NotPositive:
            return "Machine settings must be positive numbers";
        case GeometryError::NotWholeNumber:
            return "Steps/rev and pulley teeth must be whole numbers";
        case GeometryError::StepsPerMmTooLow:
            return "Steps per mm must be at least 2";
        case GeometryError::StepRateTooHigh:
            return "Max RPM x steps/rev / 60 must be at most 100000 steps/s";
        case GeometryError::StrokeTooShort:
            return "Max stroke must be longer than 50 mm";
        case GeometryError::WouldOverflow:
            return "Steps per mm is too large for the stroke or acceleration "
                   "(max about 42949)";
        case GeometryError::StorageFailed:
            return "Could not save machine settings";
    }
    return "invalid";
}

/// Clamp a requested step rate to the machine's limit. FastAccelStepper
/// ignores (rather than clamps) speeds above its own ceiling.
inline uint32_t clampStepRateHz(float requestedHz, float maxHz) {
    if (!std::isfinite(requestedHz) || requestedHz < 1.0f) return 1;
    const float limit = std::fmin(maxHz, kMaxStepRateHz);
    return static_cast<uint32_t>(std::fmin(requestedHz, limit));
}

/// Fraction of the measured stroke (0 = home, 1 = full stroke).
inline bool isValidAfterHomingPosition(float value) {
    return std::isfinite(value) && value >= 0.0f && value <= 1.0f;
}

inline bool isValidMqttPublishHz(float value) {
    return std::isfinite(value) && value >= 1.0f && value <= 100.0f;
}

}  // namespace device_config_logic
