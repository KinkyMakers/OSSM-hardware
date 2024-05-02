#ifndef STROKE_ENGINE_HELPER_H
#define STROKE_ENGINE_HELPER_H
#include <StrokeEngine.h>
#include <config.h>

#include "constants/Config.h"
#include "constants/Pins.h"

/*#################################################################################################
##
##    G L O B A L    D E F I N I T I O N S   &   D E C L A R A T I O N S
##
##################################################################################################*/

// Calculation Aid:
#define STEP_PER_REV \
    2000  // How many steps per revolution of the motor (S1 off, S2 on, S3 on,
          // S4 off)
#define PULLEY_TEETH 20  // How many teeth has the pulley
#define BELT_PITCH 2     // What is the timing belt pitch in mm
#define MAX_RPM 3000.0   // Maximum RPM of motor
#define STEP_PER_MM (STEP_PER_REV / (PULLEY_TEETH * BELT_PITCH))
#define MAX_SPEED ((MAX_RPM / 60.0) * PULLEY_TEETH * BELT_PITCH)
#define DEBUG_TALKATIVE

// enum of stroke engine states
enum StrokeEngineControl {
    STROKE,
    SENSATION,
    DEPTH
};

static motorProperties servoMotor{
    .maxSpeed =
        60 * (Config::Driver::maxSpeedMmPerSecond /
              (Config::Driver::pulleyToothCount * Config::Driver::beltPitchMm)),
    .maxAcceleration = 10000,
    .stepsPerMillimeter =
        Config::Driver::motorStepPerRevolution /
        (Config::Driver::pulleyToothCount * Config::Driver::beltPitchMm),
    .invertDirection = true,
    .enableActiveLow = true,
    .stepPin = Pins::Driver::motorStepPin,
    .directionPin = Pins::Driver::motorDirectionPin,
    .enablePin = Pins::Driver::motorEnablePin};

static endstopProperties endstop = {.homeToBack = false,
                                    .activeLow = true,
                                    .endstopPin = Pins::Driver::limitSwitchPin,
                                    .pinMode = INPUT};

static bool isChangeSignificant(float oldPct, float newPct) {
    return oldPct != newPct &&
           (abs(newPct - oldPct) > 2 || newPct == 0 || newPct == 100);
}

static float calculateSensation(float sensationPercentage) {
    return float((sensationPercentage * 200.0) / 100.0) - 100.0f;
}

#endif