#ifndef SOFTWARE_ACTIONS_H
#define SOFTWARE_ACTIONS_H

// include the ESP
#include <WiFi.h>

#include "Esp.h"
#include "esp_log.h"
#include "ossmi.h"
#include "services/wm.h"

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
#endif  // SOFTWARE_ACTIONS_H
