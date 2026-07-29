/**
 * Bounded sensorless-homing direction probe — motor moves at most 6 mm.
 *
 * Run: pio test -e hw_test -f test_hw_homing_probe
 *
 * The test samples 0.5 mm in each direction, escapes 5 mm toward the
 * lower-current direction, and disables the driver. It never starts the
 * full-stroke homing state machine.
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

    char message[384];
    snprintf(
        message, sizeof(message),
        "negative_avg=%.3f negative_peak=%.3f positive_avg=%.3f "
        "positive_peak=%.3f direction=%d limit=%.3f "
        "negative_timeout=%d positive_timeout=%d escape_avg=%.3f "
        "escape_peak=%.3f escape_hard_limit=%d escape_timeout=%d",
        diagnostics.negativeAverageLoad, diagnostics.negativePeakLoad,
        diagnostics.positiveAverageLoad, diagnostics.positivePeakLoad,
        diagnostics.direction, diagnostics.adaptiveCurrentLimit,
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
