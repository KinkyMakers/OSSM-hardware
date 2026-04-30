#ifndef OSSM_COMMUNICATION_RENAME_HPP
#define OSSM_COMMUNICATION_RENAME_HPP

#include <NimBLECharacteristic.h>
#include <NimBLEService.h>
#include <NimBLEUUID.h>
#include <Preferences.h>

std::string getDeviceName() {
    Preferences userConfig;
    userConfig.begin("UserConfig", true);
    std::string output = userConfig.getString("DeviceName","OSSM").c_str();
    userConfig.end();
    return output;
}

class RenameConfigCallbacks : public NimBLECharacteristicCallbacks {
    u32_t lastPresetCommand = millis();

    void onWrite(NimBLECharacteristic* pCharacteristic, NimBLEConnInfo& connInfo) override {
        u32_t currentTime = millis();
        if (currentTime - lastPresetCommand > 1000) {
            String value = pCharacteristic->getValue();
            value = value.substring(0,8);
            Preferences userConfig;
            userConfig.begin("UserConfig", false);
            userConfig.putString("DeviceName", value);
            userConfig.end();
            ESP_LOGI("NIMBLE_RENAME", "Rename write: %s", value.c_str());
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
