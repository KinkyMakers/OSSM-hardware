#ifndef OSSM_SOFTWARE_OSSM_H
#define OSSM_SOFTWARE_OSSM_H

#include <Arduino.h>

#include "constants/Menu.h"
#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/session.h"
#include "ossm/state/settings.h"
#include "structs/SettingPercents.h"

/**
 * OSSM class - Minimal backward compatibility shim
 *
 * This class is being deprecated in favor of:
 * - Global state singletons (settings, session, calibration, bleState, etc.)
 * - Stateless feature modules (pages::, homing::, menu::, etc.)
 *
 * The class is kept temporarily for:
 * - BLE command handling (ble_click)
 * - getCurrentState() for status reporting
 * - Static OSSM::setting for backward compatibility
 */
class OSSM {
   public:
    explicit OSSM();

    // Static settings (deprecated - use `settings` global instead)
    static SettingPercents setting;

    // BLE command handler
    void ble_click(String commandString);

    // Get current state as JSON string
    String getCurrentState(bool detailed = false);

    // Backward compatibility accessors (deprecated - use globals directly)
    int getSpeed() { return settings.speed; }

    // Menu option accessor (deprecated - use menuState.currentOption)
    Menu menuOption = Menu::SimplePenetration;

    // Session accessors (deprecated - use session.* globals)
    unsigned long sessionStartTime = 0;
    int sessionStrokeCount = 0;
    double sessionDistanceMeters = 0;
    PlayControls playControl = PlayControls::STROKE;
    bool isHomed = false;

    // Calibration accessors (deprecated - use calibration.* globals)
    float currentSensorOffset = 0;
    float measuredStrokeSteps = 0;

    // Motion targets (deprecated - use motion.* globals)
    float targetPosition = 0;
    float targetVelocity = 0;
    uint16_t targetTime = 0;

    void moveTo(float intensity, uint16_t inTime) {
        targetPosition = constrain(intensity, 0.0f, 100.0f);
        targetTime = inTime;
    }

    // BLE state accessors (deprecated - use bleState.* globals)
    bool wasLastSpeedCommandFromBLE(bool andReset = false) {
        return ::wasLastSpeedCommandFromBLE(andReset);
    }

    void resetLastSpeedCommandWasFromBLE() {
        ::resetLastSpeedCommandWasFromBLE();
    }

    bool hasActiveBLE() const { return bleState.hasActiveConnection; }

    void setBLEConnectionStatus(bool isConnected) {
        bleState.hasActiveConnection = isConnected;
    }

   private:
    bool isForward = true;
    String errorMessage = "";
    bool lastSpeedCommandWasFromBLE = false;
    bool hasActiveBLEConnection = false;
};

extern OSSM *ossm;

#endif  // OSSM_SOFTWARE_OSSM_H
