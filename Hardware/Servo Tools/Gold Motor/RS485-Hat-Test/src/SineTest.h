#pragma once

#include <Arduino.h>

#include "AimBus.h"

/**
 * Streams a swept sine to the motor as dense absolute targets (FC 0x7B) and
 * logs how well it tracks. Centre = position at start; the operator centres
 * the carriage by hand. All register changes are RAM-only and restored.
 */
struct SineParams {
    float strokeMm = 100;    // peak-to-peak
    float f0 = 0.3f;         // Hz at start
    float f1 = 3.0f;         // Hz at end (log sweep)
    float seconds = 40;
    uint16_t accel = 60000;  // 0x03 during the run: <60000 internal trapezoid, 60000+ no ramp, +NN = NN% feedforward
    float rateHz = 0;        // write rate limit, 0 = as fast as the bus allows
    uint16_t readEvery = 8;  // one telemetry read after every N writes
    float rpmCap = 1000;     // frequency stops rising once the peak speed would exceed this
    float lookaheadMs = 0;   // command the sine this far ahead of the send time
    float followMm = 8;      // abort if tracking error exceeds this
    float mmPerRev = 40;     // OSSM: 1 rev = 40 mm
};

// Parses "stroke f0 f1 seconds accel rate read_every rpm_cap lookahead_ms" (all optional, positional).
bool parseSineParams(const String &args, SineParams &p, String &error);

void runSine(AimBus &bus, const SineParams &p);
