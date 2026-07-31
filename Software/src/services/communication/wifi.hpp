#ifndef OSSM_COMMUNICATION_WIFI_HPP
#define OSSM_COMMUNICATION_WIFI_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>
#include <ArduinoJson.h>

#include "Arduino.h"
#include "communication_priority_policy.h"
#include "services/communication/priority.h"
#include "services/tasks.h"
#include "services/wm.h"

static TaskHandle_t s_legacyWifiConnectTask = nullptr;

static void legacyWifiConnectTask(void* pvParameters) {
    NimBLECharacteristic* pCharacteristic =
        static_cast<NimBLECharacteristic*>(pvParameters);
    while (!communication_priority::backgroundNetworkWorkAllowed()) {
        vTaskDelay(pdMS_TO_TICKS(
            communication_priority_policy::
                kBackgroundDeferralPollMilliseconds));
    }

    if (connectWiFi()) {
        ESP_LOGI("NIMBLE_WIFI", "WiFi connected successfully");
        pCharacteristic->setValue("ok:wifi:connected");
    } else {
        ESP_LOGW("NIMBLE_WIFI", "WiFi connection failed");
        pCharacteristic->setValue("fail:wifi:connection_failed");
    }
    s_legacyWifiConnectTask = nullptr;
    vTaskDelete(nullptr);
}

/** Handler class for WiFi configuration characteristic */
class WiFiConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        String wifiCommand = String(value.c_str());

        ESP_LOGI("NIMBLE_WIFI", "WiFi config write: %s", wifiCommand.c_str());

        // Expected format: set:wifi:<ssid>|<password>
        if (!wifiCommand.startsWith("set:wifi:")) {
            ESP_LOGW("NIMBLE_WIFI", "Invalid WiFi command format");
            pCharacteristic->setValue("fail:wifi:invalid_format");
            return;
        }

        // Extract credentials after "set:wifi:"
        String credentials = wifiCommand.substring(9); // Skip "set:wifi:"
        int pipeIndex = credentials.indexOf('|');

        if (pipeIndex == -1) {
            ESP_LOGW("NIMBLE_WIFI", "Missing pipe delimiter in WiFi command");
            pCharacteristic->setValue("fail:wifi:invalid_format");
            return;
        }

        String ssid = credentials.substring(0, pipeIndex);
        String password = credentials.substring(pipeIndex + 1);

        // Validate SSID and password
        if (ssid.length() == 0 || ssid.length() > 32) {
            ESP_LOGW("NIMBLE_WIFI", "Invalid SSID length: %d", ssid.length());
            pCharacteristic->setValue("fail:wifi:invalid_ssid");
            return;
        }

        if (password.length() < 8 || password.length() > 63) {
            ESP_LOGW("NIMBLE_WIFI", "Invalid password length: %d", password.length());
            pCharacteristic->setValue("fail:wifi:invalid_password");
            return;
        }

        if (s_legacyWifiConnectTask != nullptr) {
            ESP_LOGW("NIMBLE_WIFI", "WiFi connect already in progress");
            pCharacteristic->setValue("fail:wifi:busy");
            return;
        }

        // Save immediately, then connect on a low-priority task. In Streaming,
        // the task remains pending until BLE no longer owns the radio-priority
        // window; the existing Wi-Fi connection remains online throughout.
        if (setWiFiCredentials(ssid, password)) {
            pCharacteristic->setValue(
                communication_priority::isStreamingActive()
                    ? "ok:wifi:deferred"
                    : "ok:wifi:connecting");
            const BaseType_t created = xTaskCreatePinnedToCore(
                legacyWifiConnectTask, "legacyWifiConnect",
                4 * configMINIMAL_STACK_SIZE, pCharacteristic, 1,
                &s_legacyWifiConnectTask, Tasks::operationTaskCore);
            if (created != pdPASS) {
                s_legacyWifiConnectTask = nullptr;
                pCharacteristic->setValue("fail:wifi:task_start");
            }
        } else {
            ESP_LOGE("NIMBLE_WIFI", "Failed to save WiFi credentials");
            pCharacteristic->setValue("fail:wifi:save_failed");
        }
    }

    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        String status = getWiFiStatus();
        ESP_LOGD("NIMBLE_WIFI", "WiFi status read: %s", status.c_str());
        pCharacteristic->setValue(status);
    }

    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV(
            "NIMBLE_WIFI",
            "WiFi config notification/indication return code: %d, %s",
            code, NimBLEUtils::returnCodeToString(code));
    }
} inline wifiConfigCallbacks;

inline NimBLECharacteristic* initWiFiConfigCharacteristic(NimBLEService* pService,
                                                    NimBLEUUID uuid) {
    NimBLECharacteristic* pWiFiConfigChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    pWiFiConfigChar->setCallbacks(&wifiConfigCallbacks);
    
    // Set initial value to current WiFi status
    pWiFiConfigChar->setValue(getWiFiStatus());

    return pWiFiConfigChar;
}

#endif  // OSSM_COMMUNICATION_WIFI_HPP
