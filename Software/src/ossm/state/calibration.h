#ifndef OSSM_STATE_CALIBRATION_H
#define OSSM_STATE_CALIBRATION_H

#include <Arduino.h>

/**
 * Calibration state - tracks homing and calibration data
 * This state persists across sessions until device is re-homed
 */
struct CalibrationState {
    float currentSensorOffset = 0;
    float measuredStrokeSteps = 0;
    bool isHomed = false;
    bool isForward = true;  // Homing direction
    // True for exactly one StrokeEngine entry immediately after a physical
    // home; consumed (cleared) by startStrokeEngineTask. Gates whether the
    // StrokeEngine origin may be (re-)adopted from the current position.
    bool justHomed = false;
};

extern CalibrationState calibration;

#endif  // OSSM_STATE_CALIBRATION_H
