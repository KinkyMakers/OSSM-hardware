#ifndef OSSM_HOMING_HOMING_H
#define OSSM_HOMING_HOMING_H

namespace homing {

/**
 * Clear and prepare for homing
 * Sets up stepper parameters and resets calibration state
 */
void clearHoming();

/**
 * Start the homing task
 * Runs the sensorless homing procedure in a FreeRTOS task
 */
void startHoming();

/**
 * Perform only the bounded current probes and selected-direction escape.
 * This is public so the probe-only hardware test can validate current sensing
 * without beginning full-stroke homing.
 */
bool probeAndEscapeHardStop();

/**
 * Check if the measured stroke is too short
 * @return true if stroke is below minimum threshold
 */
bool isStrokeTooShort();

}  // namespace homing

#endif  // OSSM_HOMING_HOMING_H
