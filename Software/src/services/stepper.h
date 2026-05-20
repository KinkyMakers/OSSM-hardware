#ifndef SOFTWARE_STEPPER_H
#define SOFTWARE_STEPPER_H

#include "FastAccelStepper.h"
#include "StrokeEngine.h"
#include "constants/Pins.h"
#include "driver/rmt.h"

extern FastAccelStepperEngine stepperEngine;
extern FastAccelStepper *stepper;
extern class StrokeEngine Stroker;

void initStepper();

// Destructive teardown of FastAccelStepper for the STEP pin and bring-up of a
// dedicated RMT TX channel on the same pin (1 us tick). After this call,
// `stepper` is left in a stopped/disabled state and the FAS planner must not
// be used. DIR and ENABLE pins are reconfigured as plain GPIO outputs, with
// ENABLE driven low (motor enabled, active-low).
//
// This is a one-way operation for the current test mode; subsequent modes
// require a reboot.
void teardownStepperForRawRmt();

// RMT channel reserved by teardownStepperForRawRmt() for step pulse emission.
constexpr rmt_channel_t kRawStepRmtChannel = RMT_CHANNEL_7;

// Helpers exposed for the toppra-driven RT loop.
void rawStepSetDirection(bool forward);
void rawStepWritePulses(const rmt_item32_t *items, size_t count);

#endif  // SOFTWARE_STEPPER_H
