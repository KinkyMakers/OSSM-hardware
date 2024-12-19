#ifndef OSSM_SOFTWARE_BOARD_H
#define OSSM_SOFTWARE_BOARD_H

#include <Arduino.h>

#ifdef OSSM_CURRENT_MEAS_INA219
#include <Wire.h>
#include <Adafruit_INA219.h>

extern Adafruit_INA219 ina219;
extern TwoWire Wire_OSSM;
#endif

#include "constants/Pins.h"
#include "services/stepper.h"

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

#ifdef OSSM_CURRENT_MEAS_INA219
    ESP_LOGD("INIT","INA219 Initializing...");
    if (! ina219.begin(&Wire_OSSM)) {
        // Serial.println("Failed to find INA219 chip");
        while (1) { delay(100);  ESP_LOGD("INA", "Failed to start");}
    }
#endif

    initStepper();
}

#endif  // OSSM_SOFTWARE_BOARD_H
