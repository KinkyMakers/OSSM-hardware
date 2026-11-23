#ifndef OSSM_COMMUNICATION_STATE_HPP
#define OSSM_COMMUNICATION_STATE_HPP

#include "Arduino.h"
#include "NimBLECharacteristic.h"
#include "NimBLEService.h"
#include "NimBLEUUID.h"
#include "status_packet.h"

inline NimBLECharacteristic* initStateCharacteristic(NimBLEService* pService,
                                              NimBLEUUID uuid) {
    // State characteristic (read/notify string payload for state)
    NimBLECharacteristic* pStateChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    static const char boot_state[] PROGMEM = "ok:boot";
    pStateChar->setValue(String(FPSTR(boot_state)));

    return pStateChar;
}

// Binary status characteristic (8-byte status_packet). nimbleLoop keeps the
// value current and notifies subscribers.
inline NimBLECharacteristic* initStatusCharacteristic(NimBLEService* pService,
                                                      NimBLEUUID uuid) {
    NimBLECharacteristic* pStatusChar = pService->createCharacteristic(
        uuid, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        status_packet::kPacketSize);
    uint8_t initial[status_packet::kPacketSize] = {};
    status_packet::encode({status_packet::StateName(), 0, 0, 0, 0}, 0,
                          initial);
    pStatusChar->setValue(initial, sizeof(initial));
    return pStatusChar;
}

#endif  // OSSM_COMMUNICATION_STATE_HPP
