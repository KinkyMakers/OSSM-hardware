#ifndef OSSM_STATE_ERROR_H
#define OSSM_STATE_ERROR_H

#include <Arduino.h>

/**
 * Error state - tracks current error message
 */
struct ErrorState {
    String message = "";
};

extern ErrorState errorState;

#endif  // OSSM_STATE_ERROR_H
