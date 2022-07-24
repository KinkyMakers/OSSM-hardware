#ifndef STROKE_ENGINE_HELPER_H
#define STROKE_ENGINE_HELPER_H
#include <StrokeEngine.h>
#include <config.h>
#include "OSSM_PinDef.h"
#include "OSSM_Config.h"

/*#################################################################################################
##
##    G L O B A L    D E F I N I T I O N S   &   D E C L A R A T I O N S
##
##################################################################################################*/

// Calculation Aid:
#define STEP_PER_REV 2000 // How many steps per revolution of the motor (S1 off, S2 on, S3 on, S4 off)
#define PULLEY_TEETH 20   // How many teeth has the pulley
#define BELT_PITCH 2      // What is the timing belt pitch in mm
#define MAX_RPM 3000.0    // Maximum RPM of motor
#define STEP_PER_MM STEP_PER_REV / (PULLEY_TEETH * BELT_PITCH)
#define MAX_SPEED (MAX_RPM / 60.0) * PULLEY_TEETH* BELT_PITCH
#define DEBUG_TALKATIVE

static motorProperties servoMotor{
    .maxSpeed = 60 * (hardcode_maxSpeedMmPerSecond / (hardcode_pulleyToothCount * hardcode_beltPitchMm)),
    .maxAcceleration = 100000,
    .stepsPerMillimeter = hardcode_motorStepPerRevolution / (hardcode_pulleyToothCount * hardcode_beltPitchMm),
    .invertDirection = true,
    .enableActiveLow = true,
    .stepPin = MOTOR_STEP_PIN,
    .directionPin = MOTOR_DIRECTION_PIN,
    .enablePin = MOTOR_ENABLE_PIN};

static endstopProperties endstop = {
    .homeToBack = true, .activeLow = true, .endstopPin = LIMIT_SWITCH_PIN, .pinMode = INPUT};

#endif
