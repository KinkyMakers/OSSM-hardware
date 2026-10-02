#ifndef OSSM_SOFTWARE_ENCODER_H
#define OSSM_SOFTWARE_ENCODER_H

#include "AiEsp32RotaryEncoder.h"
#include "OneButton.h"
#include "constants/Pins.h"

// Declare the encoder as extern
extern AiEsp32RotaryEncoder encoder;

// The encoder's push switch, defined in main.cpp. Knob screens check
// isIdle() to ignore turns made while the knob is being clicked.
extern OneButton button;

// Function declarations
void IRAM_ATTR readEncoderISR();
void initEncoder();

#endif  // OSSM_SOFTWARE_ENCODER_H
