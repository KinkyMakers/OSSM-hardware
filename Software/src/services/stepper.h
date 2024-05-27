#ifndef OSSM_SOFTWARE_STEPPER_H
#define OSSM_SOFTWARE_STEPPER_H

#include <ESP_FlexyStepper.h>

#include "constants/Config.h"
#include "constants/Pins.h"
#include "services/tasks.h"

/**
 * Here are all the initialization steps for the flexyStepper motor.
 *
 * It is useful the call this function again if the motor is in an unknown
 * state.
 *
 * @param flexyStepper
 */
static void initStepper(ESP_FlexyStepper &flexyStepper) {
    flexyStepper.connectToPins(Pins::Driver::motorStepPin,
                               Pins::Driver::motorDirectionPin);
    flexyStepper.setLimitSwitchActive(Pins::Driver::limitSwitchPin);

    float stepsPerMm =
        Config::Driver::motorStepPerRevolution /
        (Config::Driver::pulleyToothCount * Config::Driver::beltPitchMm);

    flexyStepper.setStepsPerMillimeter(stepsPerMm);

    // This stepper must start on core 1, otherwise it may cause jitter.
    // https://github.com/pkerspe/ESP-FlexyStepper#a-view-words-on-jitter-in-the-generated-step-signals
    flexyStepper.startAsService(stepperCore);
}

#endif  // OSSM_SOFTWARE_STEPPER_H
