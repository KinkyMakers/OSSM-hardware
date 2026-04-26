#ifndef OSSM_SOFTWARE_ANALOG_H
#define OSSM_SOFTWARE_ANALOG_H

#include "Arduino.h"
#include "utils/AnalogSampler.h"

typedef struct {
    int pinNumber;
    int samples;
} SampleOnPin;

// Deprecated: prefer AnalogSampler::readPercent(pin) directly.
//
// Kept as a non-blocking shim around the background AnalogSampler so existing
// callers compile unchanged. The `samples` field is ignored — filtering is now
// done continuously by the sampler task at 100 Hz with an EMA per pin.
static inline float getAnalogAveragePercent(SampleOnPin sampleOnPin) {
    return AnalogSampler::readPercent(sampleOnPin.pinNumber);
}

#endif  // OSSM_SOFTWARE_ANALOG_H
