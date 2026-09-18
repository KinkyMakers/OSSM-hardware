#ifndef OSSM_STATE_BLE_H
#define OSSM_STATE_BLE_H

#include <Arduino.h>

/**
 * BLE state - tracks Bluetooth Low Energy connection and command state
 */
struct BLEState {
    bool lastSpeedCommandWasFromBLE = false;
    bool hasActiveConnection = false;
    // True while a menu action (update, pairing, restart) was requested over
    // BLE rather than the physical button. Network pages use it to report a
    // failure back over BLE instead of dropping into the captive portal.
    bool remoteMenuAction = false;
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

inline bool hasActiveBLE() {
    return bleState.hasActiveConnection;
}

inline void setBLEConnectionStatus(bool isConnected) {
    bleState.hasActiveConnection = isConnected;
}

#endif  // OSSM_STATE_BLE_H
