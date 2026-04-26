#ifndef OSSM_SOFTWARE_ANALOG_SAMPLER_H
#define OSSM_SOFTWARE_ANALOG_SAMPLER_H

#include <Arduino.h>

#include <atomic>

namespace AnalogSampler {

// Default exponential-moving-average coefficient. With the 100 Hz sampler
// (10 ms period), alpha = 0.1 gives a time constant of ~95 ms — smooth but
// still responsive to user input.
constexpr float kDefaultAlpha = 0.1f;

// Sampling cadence of the background task in milliseconds.
constexpr uint32_t kSamplePeriodMs = 10;  // 100 Hz

// Register a pin to be sampled in the background. Must be called before start()
// or pins added later will not be sampled. Safe to call multiple times for the
// same pin (subsequent calls update alpha). Returns false if the registry is
// full or sampling has already started.
bool registerPin(int pin, float alpha = kDefaultAlpha);

// Start the background sampling task. Idempotent — second call is a no-op.
void start();

// Read the latest filtered value for `pin` as a percentage in [0, 100].
// Non-blocking, safe from any task. Returns 0.0f if the pin was never
// registered.
float readPercent(int pin);

}  // namespace AnalogSampler

#endif  // OSSM_SOFTWARE_ANALOG_SAMPLER_H
