#ifndef SOFTWARE_STEPPER_H
#define SOFTWARE_STEPPER_H

#include "FastAccelStepper.h"
#include "StrokeEngine.h"
#include "constants/Pins.h"

extern FastAccelStepperEngine stepperEngine;
extern FastAccelStepper *stepper;
extern class StrokeEngine Stroker;

// ── Shared-counter coordinate frame ─────────────────────────────────────
// One position counter is shared by every mode, expressed in one of two
// frames:
//  • Native — homing / SimplePenetration / Streaming: counter 0 at the
//    homed rest position, extension = negative counts, DIR polarity normal.
//  • StrokeEngine — counter = extension − keepout (home rest reads
//    −keepout), extension = positive counts, DIR polarity inverted
//    (StrokeEngine::begin applies it, thisIsHome anchors it).
// The frames are exact mirror images: c_other = −(c + keepout_steps), the
// same formula in both directions. Homing re-establishes Native when it
// zeroes the counter; every mode entry must ensure the counter is in ITS
// frame before commanding motion.
enum class StepperFrame { Native, StrokeEngine };
extern StepperFrame stepperFrame;

// Translate the shared counter into `to` (exact math, no motion). No-op
// when already in that frame. Call only with the motor stopped.
void stepperTranslateFrame(StepperFrame to);

void initStepper();

#endif  // SOFTWARE_STEPPER_H
