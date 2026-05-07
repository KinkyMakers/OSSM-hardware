#include "stepper.h"

FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;

void initStepper() {
    stepperEngine.init();
    stepper = stepperEngine.stepperConnectToPin(Pins::Driver::motorStepPin);
    if (stepper) {
        // Path X: standardize on invertDirection=true (matches StrokeEngineHelper.h
        // and eliminates the runtime polarity flip that contaminated cross-mode
        // transitions). After this: counter increases as the rod extends, valid
        // working range is [0, +measuredStrokeSteps].
        stepper->setDirectionPin(Pins::Driver::motorDirectionPin, true);
        stepper->setEnablePin(Pins::Driver::motorEnablePin, true);
        stepper->setAutoEnable(false);
    }

    // disable motor briefly in case we are against a hard stop.
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
    delay(600);
    digitalWrite(Pins::Driver::motorEnablePin, LOW);
    delay(100);
}
