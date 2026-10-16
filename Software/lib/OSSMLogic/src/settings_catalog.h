#pragma once

#include <cctype>
#include <cstddef>
#include <cstring>

#include "device_config_logic.h"

// Persistent settings: ids, units, ranges and apply rules.
// BLE catalog strings, serial help, and docs all come from this table.
// Validation still lives in device_config_logic.h — keep the numbers matched
// (see test_settings_catalog).

namespace settings_catalog {

enum class ValueKind { Bool, Number, Integer, String };

enum class ApplyWhen { Immediate, NextHoming, AfterRestart };

struct SettingSpec {
    const char *id;
    const char *path;
    const char *valueType;
    const char *units;
    const char *rangeText;
    const char *constraintsJson;
    const char *description;
    const char *defaultText;
    ValueKind kind;
    ApplyWhen apply;
    bool persist;
    bool safetyCritical;
    float defaultNumber;
};

inline constexpr SettingSpec kSpeedLimit{
    "speed_limit",
    "setting.speedKnobAsLimit",
    "bool",
    "",
    "true|false",
    "",
    "When true, BLE speed is a percentage of the physical knob. When false, "
    "BLE speed is used directly. The legacy speedKnobLimit characteristic "
    "changes this for the current session only; this path saves it.",
    "true",
    ValueKind::Bool,
    ApplyWhen::Immediate,
    true,
    false,
    1.0f,
};

inline constexpr SettingSpec kLatency{
    "latency",
    "setting.latencyCompensation",
    "bool",
    "",
    "true|false",
    "",
    "Compensate command latency in streaming / remote motion.",
    "false",
    ValueKind::Bool,
    ApplyWhen::Immediate,
    true,
    false,
    0.0f,
};

inline constexpr SettingSpec kMetric{
    "metric",
    "setting.displayMetric",
    "bool",
    "",
    "true|false",
    "",
    "Display distances in millimetres when true, inches when false.",
    "true",
    ValueKind::Bool,
    ApplyWhen::Immediate,
    true,
    false,
    1.0f,
};

inline constexpr SettingSpec kHomePosition{
    "home_position",
    "setting.afterHomingPosition",
    "float",
    "fraction",
    "0..1",
    "{\"min\":0,\"max\":1}",
    "Where the carriage stops after homing, as a fraction of the measured "
    "stroke. 0 is the home end, 1 is full stroke. Takes effect on the next "
    "homing run.",
    "0",
    ValueKind::Number,
    ApplyWhen::NextHoming,
    true,
    false,
    0.0f,
};

inline constexpr SettingSpec kMqttRate{
    "mqtt_rate",
    "setting.mqttPublishFrequency",
    "float",
    "Hz",
    "1..100",
    "{\"min\":1,\"max\":100}",
    "MQTT telemetry publish rate, best-effort.",
    "30",
    ValueKind::Number,
    ApplyWhen::Immediate,
    true,
    false,
    30.0f,
};

inline constexpr SettingSpec kStepsPerRev{
    "steps_per_rev",
    "setting.machine.stepsPerRev",
    "int",
    "steps",
    "> 0, whole number",
    "{\"exclusiveMin\":0,\"integer\":true,\"restartRequired\":true}",
    "Stepper pulses per motor revolution, including microstepping. "
    "800 is typical for 200 steps/rev at 4x microstepping.",
    "800",
    ValueKind::Integer,
    ApplyWhen::AfterRestart,
    true,
    true,
    800.0f,
};

inline constexpr SettingSpec kPulleyTeeth{
    "pulley_teeth",
    "setting.machine.pulleyToothCount",
    "int",
    "teeth",
    "> 0, whole number",
    "{\"exclusiveMin\":0,\"integer\":true,\"restartRequired\":true}",
    "Teeth on the drive pulley.",
    "20",
    ValueKind::Integer,
    ApplyWhen::AfterRestart,
    true,
    true,
    20.0f,
};

inline constexpr SettingSpec kBeltPitch{
    "belt_pitch",
    "setting.machine.beltPitchMm",
    "float",
    "mm",
    "> 0",
    "{\"exclusiveMin\":0,\"restartRequired\":true}",
    "Belt pitch in millimetres. GT2 is 2 mm.",
    "2",
    ValueKind::Number,
    ApplyWhen::AfterRestart,
    true,
    true,
    2.0f,
};

inline constexpr SettingSpec kMaxRpm{
    "max_rpm",
    "setting.machine.maxRpm",
    "float",
    "rpm",
    "> 0",
    "{\"exclusiveMin\":0,\"maxStepRateHz\":100000,\"restartRequired\":true}",
    "Motor speed limit in revolutions per minute. Combined with steps/rev "
    "this must stay at or below 100000 steps/s.",
    "1500",
    ValueKind::Number,
    ApplyWhen::AfterRestart,
    true,
    true,
    1500.0f,
};

inline constexpr SettingSpec kMaxStroke{
    "max_stroke",
    "setting.machine.maxStrokeMm",
    "float",
    "mm",
    "> 50",
    "{\"exclusiveMin\":50,\"restartRequired\":true}",
    "Longest travel the firmware will try during homing, in millimetres. "
    "Must be longer than the 50 mm minimum stroke.",
    "500",
    ValueKind::Number,
    ApplyWhen::AfterRestart,
    true,
    true,
    500.0f,
};

inline constexpr SettingSpec kDeviceName{
    "device_name",
    "device.name",
    "string",
    "",
    "1..24 bytes, A-Z a-z 0-9 space . - _",
    "{\"maxBytes\":24,\"emptyResets\":true}",
    "Advertised BLE name. Saved immediately; the live advertisement updates "
    "on the next restart (or immediately if written through RAD BLE "
    "device.name).",
    "OSSM",
    ValueKind::String,
    ApplyWhen::AfterRestart,
    true,
    false,
    0.0f,
};

inline constexpr const SettingSpec kSettings[] = {
    kSpeedLimit, kLatency,     kMetric,     kHomePosition, kMqttRate,
    kStepsPerRev, kPulleyTeeth, kBeltPitch,  kMaxRpm,       kMaxStroke,
    kDeviceName,
};

inline constexpr size_t kSettingCount =
    sizeof(kSettings) / sizeof(kSettings[0]);

inline constexpr const char *kGeometryRules =
    "steps/mm = steps_per_rev / (pulley_teeth * belt_pitch) must be at least "
    "2; max_rpm * steps_per_rev / 60 must be at most 100000 steps/s; "
    "steps/mm * max_stroke and steps/mm * 50000 (max acceleration) must fit "
    "in a 32-bit integer (steps/mm max about 42949).";

inline constexpr size_t kDeviceNameMaxBytes = 24;

inline const SettingSpec *findById(const char *id) {
    if (id == nullptr) return nullptr;
    for (size_t i = 0; i < kSettingCount; ++i) {
        if (std::strcmp(kSettings[i].id, id) == 0) return &kSettings[i];
    }
    return nullptr;
}

inline const SettingSpec *findByPath(const char *path) {
    if (path == nullptr) return nullptr;
    for (size_t i = 0; i < kSettingCount; ++i) {
        if (std::strcmp(kSettings[i].path, path) == 0) return &kSettings[i];
    }
    return nullptr;
}

inline const char *applyWhenText(ApplyWhen when) {
    switch (when) {
        case ApplyWhen::Immediate:
            return "immediately";
        case ApplyWhen::NextHoming:
            return "next homing";
        case ApplyWhen::AfterRestart:
            return "after restart";
    }
    return "unknown";
}

inline bool isValidDeviceName(const char *name) {
    if (name == nullptr) return false;
    const size_t length = std::strlen(name);
    if (length == 0 || length > kDeviceNameMaxBytes) return false;
    for (size_t i = 0; i < length; ++i) {
        const unsigned char value = static_cast<unsigned char>(name[i]);
        if (!(std::isalnum(value) || value == ' ' || value == '-' ||
              value == '_' || value == '.'))
            return false;
    }
    return true;
}

inline device_config_logic::MachineConfig defaultMachineConfig() {
    device_config_logic::MachineConfig config;
    config.stepsPerRev = kStepsPerRev.defaultNumber;
    config.pulleyToothCount = kPulleyTeeth.defaultNumber;
    config.beltPitchMm = kBeltPitch.defaultNumber;
    config.maxRpm = kMaxRpm.defaultNumber;
    config.maxStrokeMm = kMaxStroke.defaultNumber;
    return config;
}

}  // namespace settings_catalog
