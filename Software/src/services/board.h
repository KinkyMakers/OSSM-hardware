#ifndef OSSM_SOFTWARE_BOARD_H
#define OSSM_SOFTWARE_BOARD_H

#include <Arduino.h>

#include "constants/Pins.h"

/**
 * This file changes the configuration of the board.
 */
void initBoard() {
    Serial.begin(115200);

    pinMode(Pins::Remote::encoderSwitch,
            INPUT_PULLDOWN);  // Rotary Encoder Pushbutton

    pinMode(Pins::Driver::motorEnablePin, OUTPUT);
    pinMode(Pins::Wifi::resetPin, INPUT_PULLDOWN);
    // TODO: Remove wifi toggle pin
    //    pinMode(Pins::Wifi::controlTogglePin, LOCAL_CONTROLLER); // choose
    //    between WIFI_CONTROLLER and LOCAL_CONTROLLER
    // Set analog pots (control knobs)
    pinMode(Pins::Remote::speedPotPin, INPUT);
    adcAttachPin(Pins::Remote::speedPotPin);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // allows us to read almost full 3.3V range
}

#endif  // OSSM_SOFTWARE_BOARD_H
