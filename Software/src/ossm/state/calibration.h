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
};

extern CalibrationState calibration;

#endif  // OSSM_STATE_CALIBRATION_H
