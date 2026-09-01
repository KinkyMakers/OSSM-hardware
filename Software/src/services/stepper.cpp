#include "stepper.h"

#include "constants/Config.h"

FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;
StepperFrame stepperFrame = StepperFrame::Native;

void stepperTranslateFrame(StepperFrame to) {
    if (stepperFrame == to) return;
    if (stepper == nullptr) return;
    // Mirror map between the two frames. The keepout must match
    // strokingMachine.keepoutBoundary (0 mm) in stroke_engine.cpp.
    constexpr int32_t keepoutSteps =
        static_cast<int32_t>(0.0f * Config::Driver::stepsPerMM);
    stepper->setCurrentPosition(
        -(stepper->getCurrentPosition() + keepoutSteps));
    stepperFrame = to;
}

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
