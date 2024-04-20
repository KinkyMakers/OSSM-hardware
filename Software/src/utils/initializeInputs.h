#ifndef SOFTWARE_INITIALIZEINPUTS_H
#define SOFTWARE_INITIALIZEINPUTS_H

#include "Arduino.h"
#include "OSSM_PinDef.h"

void initializeInputs() {
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    pinMode(WIFI_RESET_PIN, INPUT_PULLDOWN);
    pinMode(WIFI_CONTROL_TOGGLE_PIN,
            LOCAL_CONTROLLER);  // choose between WIFI_CONTROLLER and
                                // LOCAL_CONTROLLER
    // Set analog pots (control knobs)
    pinMode(SPEED_POT_PIN, INPUT);
    adcAttachPin(SPEED_POT_PIN);

    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);  // allows us to read almost full 3.3V range
};

#endif  // SOFTWARE_INITIALIZEINPUTS_H
