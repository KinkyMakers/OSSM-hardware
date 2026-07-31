#ifndef OSSM_STATE_BLE_H
#define OSSM_STATE_BLE_H

#include <Arduino.h>
#include <optional>

/**
 * BLE state - tracks Bluetooth Low Energy connection and command state
 */
struct BLEState {
    bool lastSpeedCommandWasFromBLE = false;
    bool hasActiveConnection = false;
};

extern BLEState bleState;

// Helper functions for BLE state management
inline bool wasLastSpeedCommandFromBLE(bool andReset = false) {
    if (andReset) {
        bool temp = bleState.lastSpeedCommandWasFromBLE;
        bleState.lastSpeedCommandWasFromBLE = false;
        return temp;
    }
    return bleState.lastSpeedCommandWasFromBLE;
}

inline void resetLastSpeedCommandWasFromBLE() {
    bleState.lastSpeedCommandWasFromBLE = false;
}

inline bool speedKnobMoved(float previous, float current) {
    constexpr float releaseThreshold = 2.0f;
    return abs(current - previous) > releaseThreshold;
}

inline float resolveBLESpeed(float speedKnob,
                             const std::optional<float> &speedBLE,
                             bool useSpeedKnobAsLimit) {
    if (!speedBLE.has_value()) return speedKnob;
    if (useSpeedKnobAsLimit) return speedKnob * speedBLE.value() / 100.0f;
    return speedBLE.value();
}

inline bool hasActiveBLE() {
    return bleState.hasActiveConnection;
}

inline void setBLEConnectionStatus(bool isConnected) {
    bleState.hasActiveConnection = isConnected;
}

#endif  // OSSM_STATE_BLE_H
