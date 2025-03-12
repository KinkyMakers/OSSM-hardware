#ifndef OSSM_SOFTWARE_ENCODER_H
#define OSSM_SOFTWARE_ENCODER_H

#include "AiEsp32RotaryEncoder.h"
#include "constants/Pins.h"

// Declare the encoder as extern
extern AiEsp32RotaryEncoder encoder;

// Function declarations
void IRAM_ATTR readEncoderISR();
void initEncoder();

#endif  // OSSM_SOFTWARE_ENCODER_H
