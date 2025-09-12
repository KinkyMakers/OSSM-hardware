#ifndef OSSM_HEADERBAR_H
#define OSSM_HEADERBAR_H

#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "services/display.h"

// BLE Status levels for 2-letter codes
enum class BleStatus {
    DISCONNECTED = 0,  // B0
    CONNECTING = 1,    // B1
    CONNECTED = 2,     // B2
    ADVERTISING = 3,   // B3
    ERROR = 4          // B4
};

// WiFi Status levels
enum class WifiStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

// Function declarations
bool shouldDrawWifiIcon();
bool shouldDrawBleIcon();
WifiStatus getWifiStatus();
BleStatus getBleStatus();
void drawWifiIcon(int16_t x, int16_t y);
void drawBleIcon(int16_t x, int16_t y);

// Task functions
[[noreturn]] void headerBarTask(void* pvParameters);
void initHeaderBar();

// Task handle
extern TaskHandle_t headerBarTaskHandle;

#endif  // OSSM_HEADERBAR_H
