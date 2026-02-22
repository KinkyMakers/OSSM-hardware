#ifndef OSSM_COMMUNICATION_COMMAND_HPP
#define OSSM_COMMUNICATION_COMMAND_HPP

#include <queue>
#include <regex>

#include "Arduino.h"
#include "NimBLECharacteristic.h"
#include "NimBLEService.h"
#include "NimBLEUUID.h"
#include "queue.h"
#include "services/led.h"

static const std::regex commandRegex(
    R"(go:(simplePenetration|strokeEngine|streaming|menu)|set:(speed|stroke|depth|sensation|buffer|pattern):\d+|set:wifi:[^|]+\|.+|stream:\d+:\d+)");

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    uint32_t lastWriteTime = 0;
    float writeHz = 0;
    const float alpha = 0.1;  // Smoothing factor for exponential moving average

    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string cmd = pCharacteristic->getValue();

        if (!std::regex_match(cmd, commandRegex)) {
            ESP_LOGD("NIMBLE_COMMAND", "Invalid command: %s", cmd.c_str());
            pCharacteristic->setValue("fail:" + String(cmd.c_str()));
            return;
        }
        messageQueue.push(String(cmd.c_str()));

        // Trigger LED communication pulse for received command
        pulseForCommunication();
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV("NIMBLE_COMMAND",
                 "Notification/Indication return code: %d, %s", code,
                 NimBLEUtils::returnCodeToString(code));
    }
} chrCallbacks;

NimBLECharacteristic* initCommandCharacteristic(NimBLEService* pService,
                                                NimBLEUUID uuid) {
    // Command characteristic (writable, readable)
    NimBLECharacteristic* pChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR);

    // Store the characteristic pointer globally

    pChar->setCallbacks(&chrCallbacks);

    return pChar;
}

#endif  // OSSM_COMMUNICATION_COMMAND_HPP
