#ifndef OSSM_STATE_SESSION_H
#define OSSM_STATE_SESSION_H

#include <Arduino.h>
#include "utils/StrokeEngineHelper.h"

/**
 * Session state - tracks the current operating session
 * This state is reset when starting a new penetration mode
 */
struct SessionState {
    unsigned long startTime = 0;
    int strokeCount = 0;
    double distanceMeters = 0;
    PlayControls playControl = PlayControls::STROKE;
};

extern SessionState session;

#endif  // OSSM_STATE_SESSION_H
