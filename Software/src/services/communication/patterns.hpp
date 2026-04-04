#ifndef OSSM_PATTERNS_HPP
#define OSSM_PATTERNS_HPP

#include "ArduinoJson.h"
#include "NimBLEService.h"
#include "constants/LogTags.h"
#include "esp_log.h"
#include "services/pattern_registry.h"

NimBLECharacteristic* initPatternsCharacteristic(NimBLEService* pService,
                                                 NimBLEUUID uuid) {
    NimBLECharacteristic* pPatternsChar =
        pService->createCharacteristic(uuid, NIMBLE_PROPERTY::READ);

    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (size_t i = 0; i < totalPatternCount; i++) {
        JsonObject pattern = arr.createNestedObject();
        pattern["name"] = patternCatalog[i].name;
        pattern["idx"] = i;
    }

    String jsonString;
    serializeJson(arr, jsonString);
    pPatternsChar->setValue(jsonString.c_str());
    return pPatternsChar;
}

class PatternDataCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        String patternValue = String(value.c_str());

        int patternIndex = patternValue.toInt();

        int validIndex =
            totalPatternCount > 0 ? patternIndex % (int)totalPatternCount : 0;
        if (validIndex < 0) {
            validIndex += (int)totalPatternCount;
        }

        const char* description = patternCatalog[validIndex].description;

        pCharacteristic->setValue(description);

        ESP_LOGD(NIMBLE_TAG,
                 "Pattern description requested: index=%d, validIndex=%d, "
                 "description=%s",
                 patternIndex, validIndex, description);
    }

    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();
        ESP_LOGD(NIMBLE_TAG, "Pattern description read: %s", value.c_str());
    }
} patternDataCallbacks;

NimBLECharacteristic* initPatternDataCharacteristic(NimBLEService* pService,
                                                    NimBLEUUID uuid) {
    NimBLECharacteristic* pPatternDataChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    pPatternDataChar->setCallbacks(&patternDataCallbacks);
    return pPatternDataChar;
}

#endif  // OSSM_PATTERNS_HPP
