#ifndef OSSM_BLE_H
#define OSSM_BLE_H

#include "NimBLECharacteristic.h"
#include "NimBLECharacteristic.h"

struct SmartLockboxState {
    uint8_t lockState;
    uint64_t lockEnd;
    uint64_t currentDate;
};

// Function to read the current state from the characteristic
static SmartLockboxState readCharacteristic(
    NimBLECharacteristic* pCharacteristic) {
    SmartLockboxState state{};
    String value = pCharacteristic->getValue();
    // EX: "1;1721284831651;1721284801651"
    // Check the format and length
    if (value.length() < 27) {  // Minimum length check
        ESP_LOGE("NimBLE", "Invalid characteristic value length");
        return state;
    }

    // Extract parts from the string
    int firstSemicolon = value.indexOf(';');
    int secondSemicolon = value.indexOf(';', firstSemicolon + 1);

    if (firstSemicolon == -1 || secondSemicolon == -1) {
        ESP_LOGE("NimBLE", "Invalid characteristic value format");
        return state;
    }

    // isLocked
    state.lockState = value.substring(0, firstSemicolon).toInt();

    // lockEnd
    String lockEndStr = value.substring(firstSemicolon + 1, secondSemicolon);
    state.lockEnd = strtoull(lockEndStr.c_str(), nullptr, 10);

    // currentDate
    String currentDateStr = value.substring(secondSemicolon + 1);
    state.currentDate = strtoull(currentDateStr.c_str(), nullptr, 10);

    // Debugging logs
    ESP_LOGD("NimBLE", "Read characteristic: %d;%s;%s", state.lockState,
             lockEndStr.c_str(), currentDateStr.c_str());
    ESP_LOGD("NimBLE", "Parsed values: %d;%llu;%llu", state.lockState,
             state.lockEnd, state.currentDate);

    return state;
}

// Function to write the current state to the characteristic
static void writeCharacteristic(NimBLECharacteristic* pCharacteristic,
                                SmartLockboxState state) {
    uint8_t value[25];
//    value[0] = state.isLocked ? 0x01 : 0x00;
    memcpy(&value[9], &state.lockEnd, sizeof(uint64_t));
    memcpy(&value[17], &state.currentDate, sizeof(uint64_t));

    pCharacteristic->setValue(value, 25);
    pCharacteristic->notify();
}

#endif  // OSSM_BLE_H
