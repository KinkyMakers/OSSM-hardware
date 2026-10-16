#ifndef OSSM_COMMUNICATION_RENAME_HPP
#define OSSM_COMMUNICATION_RENAME_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>

// rad-ble owns the name (NVS "radble"/"deviceName"). Names stored under the
// old UserConfig/DeviceName key are migrated by device_config::load().
std::string getDeviceName() {
    return radble::loadDeviceName("OSSM").c_str();
}

bool isValidLegacyDeviceName(const String& value) {
    for (size_t index = 0; index < value.length(); ++index) {
        const char character = value[index];
        const bool valid =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == ' ' ||
            character == '-' || character == '_' || character == '.';
        if (!valid) {
            return false;
        }
    }
    return true;
}

// Legacy rename characteristic. The RAD BLE device.name path renames live;
// this one restarts so the new name is used everywhere.
class RenameConfigCallbacks : public NimBLECharacteristicCallbacks {
    u32_t lastPresetCommand = millis();

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        u32_t currentTime = millis();
        if (currentTime - lastPresetCommand > 1000) {
            String value = pCharacteristic->getValue();
            value.trim();
            value = value.substring(0, radble::DEVICE_NAME_MAX_BYTES);
            if (value.isEmpty()) {
                value = "OSSM";
            }
            if (!isValidLegacyDeviceName(value)) {
                Serial.printf(
                    "[RAD BLE] device name rejected reason=invalid_characters "
                    "source=legacy\n");
                lastPresetCommand = currentTime;
                return;
            }
            const std::string oldName = getDeviceName();
            if (!radble::persistDeviceName(value)) {
                Serial.printf(
                    "[RAD BLE] device name rejected reason=persist_failed "
                    "source=legacy\n");
                lastPresetCommand = currentTime;
                return;
            }
            Serial.printf(
                "[RAD BLE] device name changed old=\"%s\" new=\"%s\" "
                "source=legacy\n",
                oldName.c_str(), value.c_str());
            ESP_LOGI("NIMBLE_RENAME", "Rename write: %s", value.c_str());
            ESP.restart();
        }
        lastPresetCommand = currentTime;
    }

    void onRead(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        std::string name = getDeviceName();
        pCharacteristic->setValue(name);
        ESP_LOGD("NIMBLE_RENAME", "Name read: %s", name.c_str());
    }
} renameConfigCallbacks;

NimBLECharacteristic* initRenameConfigCharacteristic(NimBLEService* pService, NimBLEUUID uuid) {
    NimBLECharacteristic* pRenameConfigChar = pService->createCharacteristic(
                        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    pRenameConfigChar->setCallbacks(&renameConfigCallbacks);
    pRenameConfigChar->setValue(getDeviceName());

    return pRenameConfigChar;
}

#endif  // OSSM_COMMUNICATION_RENAME_HPP
