#ifndef OSSM_NIMBLE_H
#define OSSM_NIMBLE_H

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <esp_log.h>

#include "ossm/OSSMI.h"

#define SERVICE_UUID "522b443a-4f53-534d-0001-420badbabe69"

// **********************************************************
// SYSTEM CHARACTERISTICS
// - Range: 0002-0FFF
// - Description: Reserved. Not used yet.
// **********************************************************

// **********************************************************
// Command and Configuration Characteristics
// - Range: 1000-1FFF
// - Description: These are writable and change the config of the device.
// **********************************************************
#define CHARACTERISTIC_UUID "522b443a-4f53-534d-1000-420badbabe69"
#define CHARACTERISTIC_SPEED_KNOB_CONFIG_UUID \
    "522b443a-4f53-534d-1010-420badbabe69"

// **********************************************************
// State Characteristics
// - Range: 2000-2FFF
// - Description: Read and subscribe to the current state of the
// device.
// **********************************************************

// Clients should read the current state from this char.
#define CHARACTERISTIC_STATE_UUID "522b443a-4f53-534d-2000-420badbabe69"

// ************************************************
// Pattern Characteristics
// - Range: 3000-3FFF
// - Description: Read and request information about patterns on this device.
// ************************************************
#define CHARACTERISTIC_PATTERNS_UUID "522b443a-4f53-534d-3000-420badbabe69"
#define CHARACTERISTIC_GET_PATTERN_DATA_UUID \
    "522b443a-4f53-534d-3010-420badbabe69"

// ************************************************
// GPIO Characteristics
// - Range: 4000-4FFF
// - Description: Read and request information about patterns on this device.
// ************************************************
#define CHARACTERISTIC_GPIO_UUID "522b443a-4f53-534d-4000-420badbabe69"

// ************************************************
// ****************** ETC *************************
// ************************************************
#define MANUFACTURER_NAME_UUID "2A29"    // Standard UUID for manufacturer name
#define SYSTEM_ID_UUID "2A23"            // Standard UUID for system ID
#define DEVICE_INFO_SERVICE_UUID "180A"  // Device Information Service

extern NimBLEServer* pServer;

void nimbleLoop(void* pvParameters);
void initNimble();

#endif  // OSSM_NIMBLE_H
