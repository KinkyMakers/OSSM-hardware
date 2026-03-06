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
 */
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
        ESP_LOGI("PAIRING_CHAR", "Write: %s", cmd.c_str());

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

        if (!setWiFiCredentials(ssid, password)) {
            pCharacteristic->setValue("fail:wifi:save_failed");
            return;
        }

        pCharacteristic->setValue("ok:wifi:saved");

        if (connectWiFi()) {
            ESP_LOGI("PAIRING_CHAR", "WiFi connected");
            pCharacteristic->setValue("ok:wifi:connected");
        } else {
            ESP_LOGW("PAIRING_CHAR", "WiFi connection failed");
            pCharacteristic->setValue("fail:wifi:connection_failed");
        }
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
