#include "HeaderBar.h"

#include <WiFi.h>
#include <esp_log.h>

#include "constants/Images.h"
#include "constants/LogTags.h"
#include "services/nimble.h"

// Task handle
TaskHandle_t headerBarTaskHandle = nullptr;

// Last known states for change detection
static WifiStatus lastWifiStatus = WifiStatus::DISCONNECTED;
static BleStatus lastBleStatus = BleStatus::DISCONNECTED;

// Icon positions
static const int16_t WIFI_ICON_X = 106;
static const int16_t BLE_ICON_X = 116;
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

// --- Header Bar Task ---
[[noreturn]] void headerBarTask(void* pvParameters) {
    // Initial delay to let other systems initialize
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(HEADERBAR_TAG, "Header bar task started");

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearIcons();
        drawBleIcon();
        drawWifiIcon();

        refreshIcons();
        xSemaphoreGive(displayMutex);
    }

    while (true) {
        bool shouldDrawWifi = shouldDrawWifiIcon();
        bool shouldDrawBle = shouldDrawBleIcon();

        // Only redraw if something changed
        if (!shouldDrawWifi && !shouldDrawBle) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // Take display mutex
        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            // Clear icon area
            clearIcons();
            drawBleIcon();
            drawWifiIcon();
            refreshIcons();

            // Release display mutex
            xSemaphoreGive(displayMutex);
        } else {
            ESP_LOGW(HEADERBAR_TAG, "Failed to acquire display mutex");
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void initHeaderBar() {
    ESP_LOGI(HEADERBAR_TAG, "Initializing header bar task");

    BaseType_t result =
        xTaskCreatePinnedToCore(headerBarTask, "headerBar",
                                4 * configMINIMAL_STACK_SIZE,  // Stack size
                                nullptr,
                                tskIDLE_PRIORITY + 1,  // Priority
                                &headerBarTaskHandle,
                                0  // Core 0
        );

    if (result != pdPASS) {
        ESP_LOGE(HEADERBAR_TAG, "Failed to create header bar task");
    } else {
        ESP_LOGI(HEADERBAR_TAG, "Header bar task created successfully");
    }
}
