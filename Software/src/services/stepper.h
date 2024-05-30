#ifndef SOFTWARE_STEPPER_H
#define SOFTWARE_STEPPER_H

#include "FastAccelStepper.h"
#include "constants/Pins.h"

static FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
static FastAccelStepper *stepper = nullptr;
static class StrokeEngine Stroker;

static void initStepper() {
    stepperEngine.init();
    stepper = stepperEngine.stepperConnectToPin(Pins::Driver::motorStepPin);
    if (stepper) {
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
        stepper->setEnablePin(Pins::Driver::motorEnablePin, true);
        stepper->setAutoEnable(false);
    }

    // disable motor briefly in case we are against a hard stop.
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
    delay(600);
    digitalWrite(Pins::Driver::motorEnablePin, LOW);
    delay(100);
}

#endif  // SOFTWARE_STEPPER_H
