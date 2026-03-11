#include "HeaderBar.h"

#include <WiFi.h>
#include <esp_log.h>
#include <services/board.h>

#include "constants/LogTags.h"
#include "services/communication/nimble.h"
#include "services/led.h"

TaskHandle_t headerBarTaskHandle = nullptr;
bool showHeaderIcons = true;

static WifiStatus lastWifiStatus = WifiStatus::DISCONNECTED;
static BleStatus lastBleStatus = BleStatus::DISCONNECTED;

static const int16_t WIFI_ICON_X = 104;
static const int16_t BLE_ICON_X = 118;
static const int16_t ICON_Y = 8;

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
    display.setFont(u8g2_font_siji_t_6x10);

    switch (getWifiStatus()) {
        case WifiStatus::CONNECTED:
            display.drawGlyph(WIFI_ICON_X, ICON_Y, GLYPH_WIFI_CONNECTED);
            break;
        case WifiStatus::CONNECTING:
            display.drawGlyph(WIFI_ICON_X, ICON_Y, GLYPH_WIFI_CONNECTING);
            break;
        case WifiStatus::ERROR: {
            display.drawGlyph(WIFI_ICON_X, ICON_Y, GLYPH_WIFI_OFF);
            constexpr int16_t ex = WIFI_ICON_X + 11;
            display.drawPixel(ex, ICON_Y - 5);
            display.drawPixel(ex, ICON_Y - 4);
            display.drawPixel(ex, ICON_Y - 3);
            display.drawPixel(ex, ICON_Y - 1);
            break;
        }
        case WifiStatus::DISCONNECTED:
        default:
            display.drawGlyph(WIFI_ICON_X, ICON_Y, GLYPH_WIFI_OFF);
            break;
    }
}

BleStatus getBleStatus() {
    if (pServer == nullptr) {
        return BleStatus::DISCONNECTED;
    }

    if (pServer->getAdvertising() && pServer->getConnectedCount() == 0) {
        return BleStatus::ADVERTISING;
    }

    if (pServer->getConnectedCount() > 0) {
        return BleStatus::CONNECTED;
    }

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
    display.setFont(u8g2_font_siji_t_6x10);

    static constexpr int16_t BLE_SUB_X = BLE_ICON_X - 3;
    static constexpr int16_t BLE_IND_X = BLE_SUB_X + 7;

    BleStatus status = getBleStatus();
    switch (status) {
        case BleStatus::CONNECTED:
            display.drawGlyph(BLE_ICON_X, ICON_Y, GLYPH_BLE_CONNECTED);
            break;
        case BleStatus::DISCONNECTED: {
            constexpr int16_t cx = BLE_ICON_X + 4;
            constexpr int16_t cy = ICON_Y - 4;
            display.drawCircle(cx, cy, 3);
            break;
        }
        case BleStatus::CONNECTING:
        case BleStatus::ADVERTISING:
            display.drawGlyph(BLE_SUB_X, ICON_Y, GLYPH_BLE_SMALL);
            display.drawPixel(BLE_IND_X, ICON_Y);
            display.drawPixel(BLE_IND_X + 2, ICON_Y);
            display.drawPixel(BLE_IND_X + 4, ICON_Y);
            break;
        case BleStatus::ERROR: {
            display.drawGlyph(BLE_SUB_X, ICON_Y, GLYPH_BLE_SMALL);
            constexpr int16_t ex = BLE_ICON_X + 7;
            display.drawPixel(ex, ICON_Y - 5);
            display.drawPixel(ex, ICON_Y - 4);
            display.drawPixel(ex, ICON_Y - 3);
            display.drawPixel(ex, ICON_Y - 1);
            break;
        }
    }
}

[[noreturn]] void headerBarTask(void* pvParameters) {
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    ESP_LOGI(HEADERBAR_TAG, "Header bar task started");

    if (showHeaderIcons) {
        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearIcons();
            drawWifiIcon();
            drawBleIcon();
            refreshIcons();
            xSemaphoreGive(displayMutex);
        }
    }

    bool lastShowState = showHeaderIcons;

    while (true) {
        bool stateChanged = (showHeaderIcons != lastShowState);
        lastShowState = showHeaderIcons;

        if (!showHeaderIcons) {
            if (stateChanged) {
                if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
                    clearIcons();
                    refreshIcons();
                    xSemaphoreGive(displayMutex);
                }
            }
            updateLEDForMachineStatus();
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        bool wifiChanged = shouldDrawWifiIcon() || stateChanged;
        bool bleChanged = shouldDrawBleIcon() || stateChanged;

        updateLEDForMachineStatus();

        if (!wifiChanged && !bleChanged) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearIcons();
            drawWifiIcon();
            drawBleIcon();
            refreshIcons();
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
                                4 * configMINIMAL_STACK_SIZE,
                                nullptr,
                                tskIDLE_PRIORITY + 1,
                                &headerBarTaskHandle,
                                0);

    if (result != pdPASS) {
        ESP_LOGE(HEADERBAR_TAG, "Failed to create header bar task");
    } else {
        ESP_LOGI(HEADERBAR_TAG, "Header bar task created successfully");
    }
}
