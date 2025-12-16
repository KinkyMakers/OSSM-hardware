#ifndef OSSM_TELEMETRY_H
#define OSSM_TELEMETRY_H

#include <Arduino.h>

class OSSM;

namespace Telemetry {

    void init();
    void startSession(OSSM* ossm);
    void endSession();
    bool isSessionActive();
    String getSessionId();
}

#endif
