#ifndef OSSM_HOMING_HOMING_H
#define OSSM_HOMING_HOMING_H

#include <cstdint>

namespace homing {

struct ProbeDiagnostics {
    float seedNegativeAverageLoad = 0;
    float seedNegativePeakLoad = 0;
    float seedPositiveAverageLoad = 0;
    float seedPositivePeakLoad = 0;
    float negativeAverageLoad = 0;
    float negativePeakLoad = 0;
    float positiveAverageLoad = 0;
    float positivePeakLoad = 0;
    float escapeAverageLoad = 0;
    float escapePeakLoad = 0;
    float adaptiveCurrentLimit = 0;
    int8_t direction = 0;
    bool negativeHitHardLimit = false;
    bool positiveHitHardLimit = false;
    bool negativeTimedOut = false;
    bool positiveTimedOut = false;
    bool escapeTimedOut = false;
    bool escapeHitHardLimit = false;
};

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
bool probeAndEscapeHardStop(ProbeDiagnostics* diagnostics = nullptr);

/**
 * Check if the measured stroke is too short
 * @return true if stroke is below minimum threshold
 */
bool isStrokeTooShort();

}  // namespace homing

#endif  // OSSM_HOMING_HOMING_H
