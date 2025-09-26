#ifndef OSSM_CONFIG_HPP
#define OSSM_CONFIG_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>

#include "Arduino.h"

/** Handler class for speed knob config characteristic */
class SpeedKnobConfigCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        String configValue = String(value.c_str());
        configValue.toLowerCase();

        ESP_LOGD(NIMBLE_TAG, "Speed knob config write: %s",
                 configValue.c_str());

        // Store config strings in PROGMEM
        static const char true_str[] PROGMEM = "true";
        static const char false_str[] PROGMEM = "false";
        static const char error_invalid[] PROGMEM = "error:invalid_value";

        if (configValue == "true" || configValue == "1" || configValue == "t") {
            USE_SPEED_KNOB_AS_LIMIT = true;
            pCharacteristic->setValue(String(FPSTR(true_str)));
        } else if (configValue == "false" || configValue == "0" ||
                   configValue == "f") {
            USE_SPEED_KNOB_AS_LIMIT = false;
            pCharacteristic->setValue(String(FPSTR(false_str)));
        } else {
            ESP_LOGW(NIMBLE_TAG, "Invalid speed knob config value: %s",
                     configValue.c_str());
            pCharacteristic->setValue(String(FPSTR(error_invalid)));
        }
    }

    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        // Use PROGMEM strings for read values
        static const char true_str[] PROGMEM = "true";
        static const char false_str[] PROGMEM = "false";

        String value = USE_SPEED_KNOB_AS_LIMIT ? String(FPSTR(true_str))
                                               : String(FPSTR(false_str));
        ESP_LOGD(NIMBLE_TAG, "Speed knob config read: %s", value.c_str());
        pCharacteristic->setValue(value);
    }

    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV(
            NIMBLE_TAG,
            "Speed knob config notification/indication return code: %d, %s",
            code, NimBLEUtils::returnCodeToString(code));
    }
} speedKnobConfigCallbacks;

NimBLECharacteristic* initSpeedKnobConfigCharacteristic(NimBLEService* pService,
                                                        NimBLEUUID uuid) {
    NimBLECharacteristic* pSpeedKnobConfigChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    pSpeedKnobConfigChar->setCallbacks(&speedKnobConfigCallbacks);
    // Use PROGMEM strings for initial value
    static const char true_str[] PROGMEM = "true";
    static const char false_str[] PROGMEM = "false";
    pSpeedKnobConfigChar->setValue(USE_SPEED_KNOB_AS_LIMIT
                                       ? String(FPSTR(true_str))
                                       : String(FPSTR(false_str)));

    return pSpeedKnobConfigChar;
}

#endif  // OSSM_CONFIG_HPP
