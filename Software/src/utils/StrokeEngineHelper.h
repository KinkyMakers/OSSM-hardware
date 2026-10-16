#ifndef STROKE_ENGINE_HELPER_H
#define STROKE_ENGINE_HELPER_H
#include <config.h>

#include "../lib/StrokeEngine/src/StrokeEngine.h"
#include "constants/Config.h"
#include "constants/Pins.h"

/*#################################################################################################
##
##    G L O B A L    D E F I N I T I O N S   &   D E C L A R A T I O N S
##
##################################################################################################*/

// enum of stroke engine states
enum PlayControls { STROKE, DEPTH, SENSATION, BUFFER};

// maxSpeed and stepsPerMillimeter come from the NVS machine config, which is
// loaded after static initialization; stroke_engine.cpp sets them before
// StrokeEngine::begin.
static motorProperties servoMotor{
    .maxSpeed = 0.0f,
    .maxAcceleration = Config::Driver::maxAcceleration,
    .stepsPerMillimeter = 0.0f,
    .invertDirection = true,
    .enableActiveLow = true,
    .stepPin = Pins::Driver::motorStepPin,
    .directionPin = Pins::Driver::motorDirectionPin,
    .enablePin = Pins::Driver::motorEnablePin};

static bool isChangeSignificant(float oldPct, float newPct) {
    return oldPct != newPct &&
           (abs(newPct - oldPct) > 2 || newPct == 0 || newPct == 100);
}

static float calculateSensation(float sensationPercentage) {
    return float((sensationPercentage * 200.0) / 100.0) - 100.0f;
}

#endif
