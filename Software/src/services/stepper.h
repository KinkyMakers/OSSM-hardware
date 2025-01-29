#ifndef SOFTWARE_STEPPER_H
#define SOFTWARE_STEPPER_H

#include "FastAccelStepper.h"
#include "StrokeEngine.h"
#include "constants/Pins.h"

extern FastAccelStepperEngine stepperEngine;
extern FastAccelStepper *stepper;
extern class StrokeEngine Stroker;

void initStepper();

#endif  // SOFTWARE_STEPPER_H
