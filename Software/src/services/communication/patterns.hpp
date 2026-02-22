#ifndef OSSM_PATTERNS_HPP
#define OSSM_PATTERNS_HPP

#include "ArduinoJson.h"
#include "NimBLEService.h"
#include "constants/LogTags.h"
#include "constants/UserConfig.h"
#include "esp_log.h"

NimBLECharacteristic* initPatternsCharacteristic(NimBLEService* pService,
                                                 NimBLEUUID uuid) {
    // Patterns characteristic (read-only list of all patterns)
    NimBLECharacteristic* pPatternsChar =
        pService->createCharacteristic(uuid, NIMBLE_PROPERTY::READ);
    // Use ArduinoJson to construct the patterns JSON
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < static_cast<int>(StrokePatterns::Count); i++) {
        JsonObject pattern = arr.add<JsonObject>();
        pattern["name"] = UserConfig::language.StrokeEngineNames[i];
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

        // Parse the integer input
        int patternIndex = patternValue.toInt();

        // Get the size of the StrokeEngineDescriptions array
        int descriptionsCount =
            sizeof(UserConfig::language.StrokeEngineDescriptions) /
            sizeof(UserConfig::language.StrokeEngineDescriptions[0]);

        // Use modulo to ensure valid index
        int validIndex = patternIndex % descriptionsCount;
        if (validIndex < 0) {
            validIndex += descriptionsCount;  // Handle negative numbers
        }

        // Get the pattern description from PROGMEM
        const char* description =
            UserConfig::language.StrokeEngineDescriptions[validIndex];

        // Set the characteristic value to the description
        pCharacteristic->setValue(description);

        ESP_LOGD(NIMBLE_TAG,
                 "Pattern description requested: index=%d, validIndex=%d, "
                 "description=%s",
                 patternIndex, validIndex, description);
    }

    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        // Return the current value (set by onWrite)
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
