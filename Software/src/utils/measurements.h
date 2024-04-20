#ifndef SOFTWARE_MEASUREMENTS_H
#define SOFTWARE_MEASUREMENTS_H
#include <Arduino.h>

float getAnalogAveragePercent(int pinNumber, int samples) {
    int sum = 0;
    float average;
    float percentage;
    for (int i = 0; i < samples; i++) {
        // TODO: Possibly use fancier filters?
        sum += analogRead(pinNumber);
    }
    average = float(sum) / float(samples);
    // TODO: Might want to add a deadband
    percentage = 100.0f * average / 4096.0f;  // 12 bit resolution
    return percentage;
}
#endif  // SOFTWARE_MEASUREMENTS_H
