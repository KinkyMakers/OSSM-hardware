#ifndef OSSM_STATE_STATE_H
#define OSSM_STATE_STATE_H

#include <Arduino.h>

#include "machine.h"
#include "utils/StateLogger.h"
#include <utils/RecursiveMutex.h>

extern ESP32RecursiveMutex mutex;
extern StateLogger stateLogger;

extern sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
               sml::logger<StateLogger>> *stateMachine;

// Initialize the state machine
void initStateMachine();

#endif  // OSSM_STATE_STATE_H
