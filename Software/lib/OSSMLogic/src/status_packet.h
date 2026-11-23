#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

// Pure logic for the compact binary status notification on the status
// characteristic (…2200…). No hardware dependencies — testable on native.
//
// 8 bytes, multi-byte fields little-endian, so it fits one 20-byte ATT
// payload instead of being split across several packets like the JSON:
//   [0]    state     uint8   StateName::id(), 255 (Unknown) if not listed
//   [1]    speed     uint8   0-100
//   [2]    stroke    uint8   0-100
//   [3]    depth     uint8   0-100
//   [4-5]  position  uint16  0.1 mm from home
//   [6-7]  timestamp uint16  ms since the previous status notification
//                            (capped at 65535; 0 for the first one)
// A read returns the live values with a timestamp delta of 0.

namespace status_packet {

constexpr size_t kPacketSize = 8;

/// A state machine state, as the status packet's state byte. The numbering is
/// a wire contract: only ever append; never renumber, remove, or reuse one.
class StateName {
  public:
    enum class Value : uint8_t {
        Idle = 0,
        Homing = 1,
        HomingForward = 2,
        HomingBackward = 3,
        Menu = 4,
        MenuIdle = 5,
        SimplePenetration = 6,
        SimplePenetrationIdle = 7,
        SimplePenetrationPreflight = 8,
        StrokeEngine = 9,
        StrokeEngineIdle = 10,
        StrokeEnginePreflight = 11,
        StrokeEnginePattern = 12,
        Streaming = 13,
        StreamingIdle = 14,
        StreamingPreflight = 15,
        Update = 16,
        UpdateIdle = 17,
        UpdateChecking = 18,
        UpdateAvailable = 19,
        UpdateInstalling = 20,
        UpdateFailed = 21,
        Wifi = 22,
        WifiIdle = 23,
        Help = 24,
        HelpIdle = 25,
        Error = 26,
        ErrorIdle = 27,
        ErrorHelp = 28,
        Restart = 29,
        Pairing = 30,
        PairingIdle = 31,
        PairingWifi = 32,
        PairingWifiIdle = 33,
        PairingSuccess = 34,
        PairingSuccessIdle = 35,
        PairingFailed = 36,
        Unknown = 255,
    };

    constexpr StateName() = default;
    constexpr StateName(Value value) : value_(value) {}
    /// The state with this state machine name, or Unknown.
    explicit StateName(std::string_view name) {
        for (const auto& entry : kNames) {
            if (name == entry.name) {
                value_ = entry.value;
                return;
            }
        }
    }

    constexpr Value value() const { return value_; }
    /// The state byte.
    constexpr uint8_t id() const { return static_cast<uint8_t>(value_); }
    /// The state machine name, or "unknown".
    const char* to_string() const {
        for (const auto& entry : kNames) {
            if (entry.value == value_) {
                return entry.name;
            }
        }
        return "unknown";
    }

    constexpr bool operator==(const StateName& other) const {
        return value_ == other.value_;
    }
    constexpr bool operator!=(const StateName& other) const {
        return value_ != other.value_;
    }

    struct Entry {
        Value value;
        const char* name;
    };
    static constexpr Entry kNames[] = {
        {Value::Idle, "idle"},
        {Value::Homing, "homing"},
        {Value::HomingForward, "homing.forward"},
        {Value::HomingBackward, "homing.backward"},
        {Value::Menu, "menu"},
        {Value::MenuIdle, "menu.idle"},
        {Value::SimplePenetration, "simplePenetration"},
        {Value::SimplePenetrationIdle, "simplePenetration.idle"},
        {Value::SimplePenetrationPreflight, "simplePenetration.preflight"},
        {Value::StrokeEngine, "strokeEngine"},
        {Value::StrokeEngineIdle, "strokeEngine.idle"},
        {Value::StrokeEnginePreflight, "strokeEngine.preflight"},
        {Value::StrokeEnginePattern, "strokeEngine.pattern"},
        {Value::Streaming, "streaming"},
        {Value::StreamingIdle, "streaming.idle"},
        {Value::StreamingPreflight, "streaming.preflight"},
        {Value::Update, "update"},
        {Value::UpdateIdle, "update.idle"},
        {Value::UpdateChecking, "update.checking"},
        {Value::UpdateAvailable, "update.available"},
        {Value::UpdateInstalling, "update.installing"},
        {Value::UpdateFailed, "update.failed"},
        {Value::Wifi, "wifi"},
        {Value::WifiIdle, "wifi.idle"},
        {Value::Help, "help"},
        {Value::HelpIdle, "help.idle"},
        {Value::Error, "error"},
        {Value::ErrorIdle, "error.idle"},
        {Value::ErrorHelp, "error.help"},
        {Value::Restart, "restart"},
        {Value::Pairing, "pairing"},
        {Value::PairingIdle, "pairing.idle"},
        {Value::PairingWifi, "pairing.wifi"},
        {Value::PairingWifiIdle, "pairing.wifi.idle"},
        {Value::PairingSuccess, "pairing.success"},
        {Value::PairingSuccessIdle, "pairing.success.idle"},
        {Value::PairingFailed, "pairing.failed"},
    };

  private:
    Value value_ = Value::Unknown;
};

struct Status {
    StateName state;
    uint8_t speed;
    uint8_t stroke;
    uint8_t depth;
    uint16_t positionTenthsMm;

    bool operator==(const Status& other) const {
        return state == other.state && speed == other.speed &&
               stroke == other.stroke && depth == other.depth &&
               positionTenthsMm == other.positionTenthsMm;
    }
    bool operator!=(const Status& other) const { return !(*this == other); }
};

/// A 0-100 setting -> its status byte, rounded. NaN gives 0 (std::clamp
/// passes NaN through, and converting NaN to an integer is undefined).
inline uint8_t clampPercent(float value) {
    if (std::isnan(value)) {
        return 0;
    }
    return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0f, 100.0f)));
}

/// Distance from home in mm -> 0.1 mm units, clamped to the uint16 range.
inline uint16_t positionTenths(float distanceFromHomeMm) {
    const float tenths = distanceFromHomeMm * 10.0f;
    if (std::isnan(tenths)) {
        return 0;
    }
    return static_cast<uint16_t>(std::lround(std::clamp(
        tenths, 0.0f,
        static_cast<float>(std::numeric_limits<uint16_t>::max()))));
}

/// Milliseconds between two millis() readings, capped at 65535.
inline uint16_t timestampDelta(uint32_t nowMs, uint32_t previousMs) {
    uint32_t delta = nowMs - previousMs;  // wraps correctly
    return static_cast<uint16_t>(std::min<uint32_t>(
        delta, std::numeric_limits<uint16_t>::max()));
}

inline void encode(const Status& status, uint16_t deltaMs,
                   uint8_t out[kPacketSize]) {
    out[0] = status.state.id();
    out[1] = status.speed;
    out[2] = status.stroke;
    out[3] = status.depth;
    out[4] = static_cast<uint8_t>(status.positionTenthsMm & 0xFF);
    out[5] = static_cast<uint8_t>(status.positionTenthsMm >> 8);
    out[6] = static_cast<uint8_t>(deltaMs & 0xFF);
    out[7] = static_cast<uint8_t>(deltaMs >> 8);
}

// Send policy, matching the JSON state notifications: immediately when the
// state or a setting changes, otherwise a heartbeat every kHeartbeatMs.
// Position alone never triggers a send — that keeps BLE traffic down; it
// goes out with the next heartbeat or change, or a client can read it.
constexpr uint32_t kHeartbeatMs = 1000;

inline bool settingsOrStateChanged(const Status& a, const Status& b) {
    return a.state != b.state || a.speed != b.speed || a.stroke != b.stroke ||
           a.depth != b.depth;
}

inline bool shouldSend(bool changed, uint32_t msSinceLastSend) {
    return changed || msSinceLastSend >= kHeartbeatMs;
}

}  // namespace status_packet
