//
// Created by Andrew Koenig on 2023-12-09.
//

#ifndef SOFTWARE_ENCODER_H
#define SOFTWARE_ENCODER_H

#include "AiEsp32RotaryEncoder.h"
#include "OSSM_PinDef.h"
static AiEsp32RotaryEncoder g_encoder = AiEsp32RotaryEncoder(ENCODER_A, ENCODER_B, ENCODER_SWITCH, -1, 2);

#endif // SOFTWARE_ENCODER_H
