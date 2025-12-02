#ifndef OSSM_TELEMETRY_H
#define OSSM_TELEMETRY_H

#include <Arduino.h>

#include "FastAccelStepper.h"

namespace Telemetry {

    // Session management
    void startSession(FastAccelStepper* stepper);
    void endSession();
    bool isSessionActive();
    String getSessionId();

    // Initialize telemetry system (call once at startup)
    void init(FastAccelStepper* stepper);

}

#endif
