#ifndef OSSM_COMMUNICATION_PAIRING_HPP
#define OSSM_COMMUNICATION_PAIRING_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>
#include <WiFi.h>

#include "constants/Version.h"
#include "services/wm.h"

/**
 * Pairing characteristic — used by the RAD dashboard BLE one-click pairing
 * flow. Mirrors the Lockbox pairing protocol so the dashboard can reuse the
 * same useBLE hook with a different config.
 *
 * Read:  "MAC;chip;wifiConnected;md5;version"
 *        wifiConnected: "1" = connected, "0" = not connected
 *
 * Write: "9;ssid;password"  — provisions WiFi credentials
 *        Returns "ok:wifi:connecting" immediately; poll the read characteristic
 *        for wifiConnected=1, or read back "ok:wifi:connected" /
 *        "fail:wifi:connection_failed" once the task completes.
 */

// Guard against a second write while a connect attempt is already in flight.
static TaskHandle_t s_wifiConnectTask = nullptr;

static void wifiConnectTask(void* pvParameters) {
    NimBLECharacteristic* pChar =
        static_cast<NimBLECharacteristic*>(pvParameters);

    if (connectWiFi()) {
        ESP_LOGI("PAIRING_CHAR", "WiFi connected");
        pChar->setValue("ok:wifi:connected");
    } else {
        ESP_LOGW("PAIRING_CHAR", "WiFi connection failed");
        pChar->setValue("fail:wifi:connection_failed");
    }

    s_wifiConnectTask = nullptr;
    vTaskDelete(nullptr);
}

class PairingCallbacks : public NimBLECharacteristicCallbacks {
    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        uint8_t wifiConnected = (WiFi.status() == WL_CONNECTED) ? 1 : 0;
        String info = WiFi.macAddress() + ";" + ESP.getChipModel() + ";" +
                      String(wifiConnected) + ";" + ESP.getSketchMD5() + ";" +
                      String(VERSION);
        pCharacteristic->setValue(info);
        ESP_LOGD("PAIRING_CHAR", "Read: %s", info.c_str());
    }

    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        String cmd = String(value.c_str());

        // Expected format: "9;ssid;password"
        if (!cmd.startsWith("9;")) {
            ESP_LOGW("PAIRING_CHAR", "Unknown pairing command");
            pCharacteristic->setValue("fail:unknown_command");
            return;
        }

        String rest = cmd.substring(2);
        int sep = rest.indexOf(';');
        if (sep == -1) {
            ESP_LOGW("PAIRING_CHAR", "Invalid format — missing separator");
            pCharacteristic->setValue("fail:invalid_format");
            return;
        }

        String ssid = rest.substring(0, sep);
        String password = rest.substring(sep + 1);

        if (ssid.length() == 0) {
            pCharacteristic->setValue("fail:invalid_ssid");
            return;
        }

        ESP_LOGI("PAIRING_CHAR", "WiFi provisioning: ssid='%s'", ssid.c_str());

        if (!setWiFiCredentials(ssid, password)) {
            pCharacteristic->setValue("fail:wifi:save_failed");
            return;
        }

        if (s_wifiConnectTask != nullptr) {
            ESP_LOGW("PAIRING_CHAR", "WiFi connect already in progress");
            pCharacteristic->setValue("fail:wifi:busy");
            return;
        }

        // Return immediately so the NimBLE host task is not blocked.
        // wifiConnectTask will update the characteristic value when done.
        pCharacteristic->setValue("ok:wifi:connecting");
        xTaskCreate(wifiConnectTask, "wifiConnect",
                    4 * configMINIMAL_STACK_SIZE, pCharacteristic, 1,
                    &s_wifiConnectTask);
    }
} pairingCallbacks;

NimBLECharacteristic* initPairingCharacteristic(NimBLEService* pService,
                                                 NimBLEUUID uuid) {
    NimBLECharacteristic* pPairingChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    pPairingChar->setCallbacks(&pairingCallbacks);
    return pPairingChar;
}

#endif  // OSSM_COMMUNICATION_PAIRING_HPP
