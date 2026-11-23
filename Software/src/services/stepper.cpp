#include "stepper.h"

#include "constants/Config.h"

FastAccelStepperEngine stepperEngine = FastAccelStepperEngine();
FastAccelStepper *stepper = nullptr;
class StrokeEngine Stroker;
StepperFrame stepperFrame = StepperFrame::Native;

// Mirror map between the two frames. The keepout must match
// strokingMachine.keepoutBoundary (6 mm) in stroke_engine.cpp.
static constexpr int32_t keepoutSteps =
    static_cast<int32_t>(6.0f * Config::Driver::stepsPerMM);

void stepperTranslateFrame(StepperFrame to) {
    if (stepperFrame == to) {
        return;
    }
    if (stepper == nullptr) {
        return;
    }
    stepper->setCurrentPosition(
        -(stepper->getCurrentPosition() + keepoutSteps));
    stepperFrame = to;
}

int32_t stepperDistanceFromHomeSteps() {
    if (stepper == nullptr) {
        return 0;
    }
    const int32_t counter = stepper->getCurrentPosition();
    // Native: extension is negative. StrokeEngine: counter = extension - keepout.
    return stepperFrame == StepperFrame::Native ? -counter
                                                : counter + keepoutSteps;
}

void initStepper() {
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
