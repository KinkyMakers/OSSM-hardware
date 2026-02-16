#ifndef OSSM_COMMUNICATION_WIFI_HPP
#define OSSM_COMMUNICATION_WIFI_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>
#include <ArduinoJson.h>

#include "Arduino.h"
#include "services/wm.h"

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

        ESP_LOGI("NIMBLE_WIFI", "Setting WiFi credentials - SSID: %s", ssid.c_str());

        // Save credentials and attempt connection
        if (setWiFiCredentials(ssid, password)) {
            pCharacteristic->setValue("ok:wifi:saved");
            
            // Attempt to connect
            if (connectWiFi()) {
                ESP_LOGI("NIMBLE_WIFI", "WiFi connected successfully");
                pCharacteristic->setValue("ok:wifi:connected");
            } else {
                ESP_LOGW("NIMBLE_WIFI", "WiFi connection failed");
                pCharacteristic->setValue("fail:wifi:connection_failed");
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
} wifiConfigCallbacks;

NimBLECharacteristic* initWiFiConfigCharacteristic(NimBLEService* pService,
                                                    NimBLEUUID uuid) {
    NimBLECharacteristic* pWiFiConfigChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    pWiFiConfigChar->setCallbacks(&wifiConfigCallbacks);
    
    // Set initial value to current WiFi status
    pWiFiConfigChar->setValue(getWiFiStatus());

    return pWiFiConfigChar;
}

#endif  // OSSM_COMMUNICATION_WIFI_HPP
