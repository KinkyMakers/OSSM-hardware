#ifndef SOFTWARE_GUARD_H
#define SOFTWARE_GUARD_H

#include "WiFi.h"
#include "constants/Menu.h"
static auto isOnline = []() {
    bool online = WiFiClass::status() == WL_CONNECTED;
    ESP_LOGI("GUARD", "isOnline: %s (status=%d)", online ? "true" : "false",
             WiFiClass::status());
    return online;
};

#endif  // SOFTWARE_GUARD_H
