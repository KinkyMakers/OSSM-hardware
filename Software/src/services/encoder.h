#ifndef OSSM_SOFTWARE_ENCODER_H
#define OSSM_SOFTWARE_ENCODER_H

#include "AiEsp32RotaryEncoder.h"
#include "constants/Pins.h"

static AiEsp32RotaryEncoder encoder = AiEsp32RotaryEncoder(
    Pins::Remote::encoderA, Pins::Remote::encoderB, Pins::Remote::encoderSwitch,
    Pins::Remote::encoderPower, Pins::Remote::encoderStepsPerNotch);

static void IRAM_ATTR readEncoderISR() { encoder.readEncoder_ISR(); }

static void initEncoder() {
    // we must initialize rotary encoder
    encoder.begin();
    encoder.setup(readEncoderISR);
    // set boundaries and if values should cycle or not
    // in this example we will set possible values between 0 and 1000;
    encoder.setBoundaries(
        0, 99, false);  // minValue, maxValue, circleValues true|false (when max
                        // go to min and vice versa)
    encoder.setAcceleration(0);
}

#endif  // OSSM_SOFTWARE_ENCODER_H
