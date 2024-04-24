#ifndef SOFTWARE_ACTIONS_H
#define SOFTWARE_ACTIONS_H

// include the ESP
#include <WiFi.h>

#include "Esp.h"
#include "esp_log.h"
#include "ossmi.h"
#include "services/wm.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/stepper.h"

auto restart = []() { ESP.restart(); };

auto startWifi = []() {
    if (WiFiClass::status() == WL_CONNECTED) {
        return;
    }
    // Start the wifi task.

    // If you have saved wifi credentials then connect to wifi
    // immediately.
    wm.setConfigPortalTimeout(1);
    wm.setConnectTimeout(1);
    wm.setConnectRetries(1);
    wm.setConfigPortalBlocking(false);
    if (!wm.autoConnect()) {
        ESP_LOGD("UTILS", "failed to connect and hit timeout");
    }
    ESP_LOGD("UTILS", "exiting autoconnect");
};

auto stopWifiPortal = []() { wm.stopConfigPortal(); };

auto startHoming = [](OSSMI &o) {
    o.clearHoming();
    o.startHoming();
};

auto reverseHoming = [](OSSMI &o) {
    o.reverseHoming();
};

#endif  // SOFTWARE_ACTIONS_H
