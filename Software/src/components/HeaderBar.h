#ifndef OSSM_HEADERBAR_H
#define OSSM_HEADERBAR_H

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "services/display.h"

enum class BleStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ADVERTISING = 3,
    ERROR = 4
};

enum class WifiStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

constexpr uint16_t GLYPH_WIFI_OFF = 0xe218;
constexpr uint16_t GLYPH_WIFI_CONNECTING = 0xe219;
constexpr uint16_t GLYPH_WIFI_CONNECTED = 0xe21a;
constexpr uint16_t GLYPH_WIFI_ERROR = 0xe21b;
constexpr uint16_t GLYPH_BLE_CONNECTED = 0xe00b;
constexpr uint16_t GLYPH_BLE_SMALL = 0xe0b0;
constexpr uint16_t GLYPH_EXCLAMATION = 0xe0b3;

bool shouldDrawWifiIcon();
bool shouldDrawBleIcon();
WifiStatus getWifiStatus();
BleStatus getBleStatus();
void drawWifiIcon();
void drawBleIcon();

[[noreturn]] void headerBarTask(void* pvParameters);
void initHeaderBar();

extern TaskHandle_t headerBarTaskHandle;
extern bool showHeaderIcons;

#endif  // OSSM_HEADERBAR_H
