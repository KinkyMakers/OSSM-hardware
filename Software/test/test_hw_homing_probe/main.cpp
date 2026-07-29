/**
 * Bounded sensorless-homing direction probe — no full homing travel.
 *
 * Run: pio test -e hw_test -f test_hw_homing_probe
 *
 * The test seeds the current threshold with 0.5 mm samples, attempts a
 * current-limited 5 mm wiggle in each direction, escapes 5 mm toward the
 * unblocked direction, and disables the driver. It commands at most 16 mm
 * cumulatively and never starts the full-stroke homing state machine.
 */

#include <Arduino.h>
#include <unity.h>

#include "ossm/homing/homing.h"
#include "services/board.h"
#include "services/led.h"
#include "services/stepper.h"

void setUp(void) {}

void tearDown(void) {
    if (stepper != nullptr) {
        stepper->forceStop();
        stepper->disableOutputs();
    }
    setHomingActive(false);
}

void test_bounded_probe_finds_escape_direction(void) {
    homing::clearHoming();
    stepper->enableOutputs();
    homing::ProbeDiagnostics diagnostics;
    const bool safeDirectionFound =
        homing::probeAndEscapeHardStop(&diagnostics);
    stepper->forceStop();
    stepper->disableOutputs();

    char message[768];
    snprintf(
        message, sizeof(message),
        "seed_negative_avg=%.3f seed_negative_peak=%.3f "
        "seed_positive_avg=%.3f seed_positive_peak=%.3f limit=%.3f "
        "negative_avg=%.3f negative_peak=%.3f negative_blocked=%d "
        "positive_avg=%.3f positive_peak=%.3f positive_blocked=%d "
        "direction=%d negative_timeout=%d positive_timeout=%d "
        "escape_avg=%.3f escape_peak=%.3f escape_hard_limit=%d "
        "escape_timeout=%d",
        diagnostics.seedNegativeAverageLoad,
        diagnostics.seedNegativePeakLoad,
        diagnostics.seedPositiveAverageLoad,
        diagnostics.seedPositivePeakLoad,
        diagnostics.adaptiveCurrentLimit,
        diagnostics.negativeAverageLoad, diagnostics.negativePeakLoad,
        diagnostics.negativeHitHardLimit,
        diagnostics.positiveAverageLoad, diagnostics.positivePeakLoad,
        diagnostics.positiveHitHardLimit, diagnostics.direction,
        diagnostics.negativeTimedOut, diagnostics.positiveTimedOut,
        diagnostics.escapeAverageLoad, diagnostics.escapePeakLoad,
        diagnostics.escapeHitHardLimit, diagnostics.escapeTimedOut);
    TEST_ASSERT_TRUE_MESSAGE(safeDirectionFound, message);
}

void setup() {
    delay(2000);
    initBoard();

    UNITY_BEGIN();
    RUN_TEST(test_bounded_probe_finds_escape_direction);
    UNITY_END();
}

void loop() {}
