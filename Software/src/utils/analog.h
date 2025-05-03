#ifndef OSSM_SOFTWARE_ANALOG_H
#define OSSM_SOFTWARE_ANALOG_H

#include "Arduino.h"

typedef struct {
    int pinNumber;
    int samples;
} SampleOnPin;

// public static function to get the analog value of a pin
static float getAnalogAveragePercent(SampleOnPin sampleOnPin, float deadzonePercentage = 0.0f) {
    int sum = 0;
    float average;
    float percentage;

    for (int i = 0; i < sampleOnPin.samples; i++) {
        // TODO: Possibly use fancier filters?
        sum += analogRead(sampleOnPin.pinNumber);
    }
    average = (float)sum / (float)sampleOnPin.samples;

    // Set up deadzone clamped to 0-100%
    // This is to prevent noise from the analog pin from causing false readings
    // This deadzone is only applied at the lower end of the speed knob
    float commandDeadZonePercentage =
        (deadzonePercentage < 0.0001f)
            ? 0.0f
            : (deadzonePercentage > 100.0f)
                  ? 100.0f
                  : deadzonePercentage;

    // Using the CommandDeadZonePercentage, calculate a percentage that spans 0-100 but excludes the lower dead zone
    if (average < (commandDeadZonePercentage * 4096.0f / 100.0f)) {
        average = 0.0f; // Set to zero if below the dead zone
    } else {
        average = (average - (commandDeadZonePercentage * 4096.0f / 100.0f)) /
                  (4096.0f - (commandDeadZonePercentage * 4096.0f / 100.0f)) * 4096.0f;
    }

    percentage = 100.0f * average / 4096.0f;  // 12 bit resolution
    return percentage;
}

#endif  // OSSM_SOFTWARE_ANALOG_H
