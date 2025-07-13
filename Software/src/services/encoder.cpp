#include "encoder.h"

// Define the global encoder instance
AiEsp32RotaryEncoder encoder(
    Pins::Remote::encoderA, 
    Pins::Remote::encoderB, 
    Pins::Remote::encoderSwitch,
    Pins::Remote::encoderPower, 
    Pins::Remote::encoderStepsPerNotch
);

void IRAM_ATTR readEncoderISR() { 
    encoder.readEncoder_ISR(); 
}

void initEncoder() {
    encoder.begin();
    encoder.setup(readEncoderISR);
    encoder.setBoundaries(0, 99, false);
    encoder.setAcceleration(0);
    encoder.disableAcceleration();
} 