#include "HeaderBar.h"

#include <WiFi.h>
#include <esp_log.h>
#include <services/board.h>
#include <services/display.h>

#include "constants/Images.h"
#include "constants/LogTags.h"
#include "constants/UserConfig.h"
#include "services/communication/nimble.h"

// Task handle
TaskHandle_t headerBarTaskHandle = nullptr;

// Last known states for change detection
static WifiStatus lastWifiStatus = WifiStatus::DISCONNECTED;
static BleStatus lastBleStatus = BleStatus::DISCONNECTED;
static bool lastSpeedKnobAsLimit = true;

// Icon positions
static const int16_t WIFI_ICON_X = 106;
static const int16_t BLE_ICON_X = 116;
static const int16_t SPEED_KNOB_ICON_X = 96;
static const int16_t ICON_Y = 0;
static const int16_t ICON_SIZE = 8;

// --- WiFi Status Functions ---
WifiStatus getWifiStatus() {
    wl_status_t wifiStatus = WiFiClass::status();

    switch (wifiStatus) {
        case WL_CONNECTED:
            return WifiStatus::CONNECTED;
        case WL_IDLE_STATUS:
            return WifiStatus::CONNECTING;
        case WL_NO_SSID_AVAIL:
        case WL_CONNECT_FAILED:
        case WL_DISCONNECTED:
        default:
            return WifiStatus::DISCONNECTED;
    }
}

bool shouldDrawWifiIcon() {
    WifiStatus currentStatus = getWifiStatus();
    if (currentStatus != lastWifiStatus) {
        lastWifiStatus = currentStatus;
        return true;
    }
    return false;
}

void drawWifiIcon() {
    WifiStatus status = getWifiStatus();

    switch (status) {
        case WifiStatus::CONNECTED:
            display.drawXBMP(WIFI_ICON_X, ICON_Y, WifiIcon::w, WifiIcon::h,
                             WifiIcon::Connected);
            break;
        case WifiStatus::CONNECTING:
            display.drawXBMP(WIFI_ICON_X, ICON_Y, WifiIcon::w, WifiIcon::h,
                             WifiIcon::First);
            break;
        case WifiStatus::DISCONNECTED:
        case WifiStatus::ERROR:
        default:
            display.drawXBMP(WIFI_ICON_X, ICON_Y, WifiIcon::w, WifiIcon::h,
                             WifiIcon::Error);
            break;
    }
}

// --- BLE Status Functions ---
BleStatus getBleStatus() {
    if (pServer == nullptr) {
        return BleStatus::DISCONNECTED;
    }

    // Check if BLE is advertising
    if (pServer->getAdvertising() && pServer->getConnectedCount() == 0) {
        return BleStatus::ADVERTISING;
    }

    // Check if BLE has connections
    if (pServer->getConnectedCount() > 0) {
        return BleStatus::CONNECTED;
    }

    // Check if BLE is in connecting state (advertising but no connections yet)
    if (pServer->getAdvertising()) {
        return BleStatus::CONNECTING;
    }

    return BleStatus::DISCONNECTED;
}

bool shouldDrawBleIcon() {
    BleStatus currentStatus = getBleStatus();
    if (currentStatus != lastBleStatus) {
        lastBleStatus = currentStatus;
        return true;
    }
    return false;
}

void drawBleIcon() {
    BleStatus status = getBleStatus();

    // Set font for 2-letter codes
    display.setFont(u8g2_font_spleen5x8_mu);

    // Draw 2-letter status code
    switch (status) {
        case BleStatus::DISCONNECTED:
            display.drawStr(BLE_ICON_X, ICON_Y + 7, "B0");  // Disconnected
            break;
        case BleStatus::CONNECTING:
            display.drawStr(BLE_ICON_X, ICON_Y + 7, "B1");  // Connecting
            break;
        case BleStatus::CONNECTED:
            display.drawStr(BLE_ICON_X, ICON_Y + 7, "B2");  // Connected
            break;
        case BleStatus::ADVERTISING:
            display.drawStr(BLE_ICON_X, ICON_Y + 7, "B3");  // Advertising
            break;
        case BleStatus::ERROR:
        default:
            display.drawStr(BLE_ICON_X, ICON_Y + 7, "B4");  // Error
            break;
    }
}

// --- Speed Knob Status Functions ---
bool shouldDrawSpeedKnobIcon() {
    bool currentSpeedKnobAsLimit = USE_SPEED_KNOB_AS_LIMIT;
    if (currentSpeedKnobAsLimit != lastSpeedKnobAsLimit) {
        lastSpeedKnobAsLimit = currentSpeedKnobAsLimit;
        return true;
    }
    return false;
}

void drawSpeedKnobIcon() {
    // Set font for 2-letter codes
    display.setFont(u8g2_font_spleen5x8_mu);

    // Draw 2-letter status code
    if (USE_SPEED_KNOB_AS_LIMIT) {
        display.drawStr(SPEED_KNOB_ICON_X, ICON_Y + 7,
                        "S1");  // Speed knob as limit enabled
    } else {
        display.drawStr(SPEED_KNOB_ICON_X, ICON_Y + 7,
                        "S0");  // Speed knob as limit disabled
    }
}

// --- Header Bar Task ---
[[noreturn]] void headerBarTask(void* pvParameters) {
    // Wait much longer to avoid any interference with startup
    vTaskDelay(15000 / portTICK_PERIOD_MS);

    ESP_LOGI(HEADERBAR_TAG, "Header bar task started (simplified version)");

    while (true) {
        // Much longer delay between updates to minimize interference
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        
        // Only update if we can get the mutex quickly, otherwise skip
        if (xSemaphoreTake(displayMutex, 10) == pdTRUE) {
            // Very minimal update - just check status but don't actually draw
            bool wifiChanged = shouldDrawWifiIcon();
            bool bleChanged = shouldDrawBleIcon();
            bool speedChanged = shouldDrawSpeedKnobIcon();
            
            // Log status changes but don't actually draw to avoid corruption
            if (wifiChanged || bleChanged || speedChanged) {
                ESP_LOGD(HEADERBAR_TAG, "Status change detected (wifi=%d, ble=%d, speed=%d)", 
                         wifiChanged, bleChanged, speedChanged);
            }
            
            xSemaphoreGive(displayMutex);
        }
    }
}

void initHeaderBar() {
    ESP_LOGD(HEADERBAR_TAG, "initializing Header Bar");

    int stackSize = 2 * configMINIMAL_STACK_SIZE;
    xTaskCreatePinnedToCore(headerBarTask, "headerBarTask", stackSize,
                            nullptr, 1, &headerBarTaskHandle,
                            0); // Use core 0 for display tasks
}
