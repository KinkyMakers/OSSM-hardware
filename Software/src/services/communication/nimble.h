#ifndef OSSM_NIMBLE_H
#define OSSM_NIMBLE_H

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <esp_log.h>

#include "ossm/OSSMI.h"

#define SERVICE_UUID "522b443a-4f53-534d-0001-420badbabe69"

// Clients should write to this char.
#define CHARACTERISTIC_UUID "522b443a-4f53-534d-0002-420badbabe69"
// Speed knob as limit configuration (writable, readable)
#define CHARACTERISTIC_SPEED_KNOB_CONFIG_UUID \
    "522b443a-4f53-534d-0010-420badbabe69"

// Clients should read the current state from this char.
#define CHARACTERISTIC_STATE_UUID "522b443a-4f53-534d-1000-420badbabe69"

// this is a list of all the patterns on the device.
#define CHARACTERISTIC_PATTERNS_UUID "522b443a-4f53-534d-2000-420badbabe69"

#define MANUFACTURER_NAME_UUID "2A29"    // Standard UUID for manufacturer name
#define SYSTEM_ID_UUID "2A23"            // Standard UUID for system ID
#define DEVICE_INFO_SERVICE_UUID "180A"  // Device Information Service

extern NimBLEServer* pServer;

void nimbleLoop(void* pvParameters);
void initNimble();

#endif  // OSSM_NIMBLE_H
