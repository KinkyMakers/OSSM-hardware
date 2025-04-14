#ifndef OSSM_NIMBLE_H
#define OSSM_NIMBLE_H

#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <esp_log.h>

#include <queue>

#include "ossm/OSSMI.h"

struct Target {
    uint16_t position;
    int16_t velocity;
};

extern NimBLEServer* pServer;

// Global variables for target position and velocity
extern uint16_t nimbleTargetPosition;
extern int16_t nimbleTargetVelocity;

extern std::queue<Target> targetQueue;

void nimbleLoop(void* pvParameters);
void initNimble();

#endif  // OSSM_NIMBLE_H
