/**
 * Bounded sensorless-homing direction probe — no full homing travel.
 *
 * Run: pio test -e hw_test -f test_hw_homing_probe
 *
 * The test moves 5 mm behind the original position and then 10 mm forward to
 * 5 mm ahead of that origin at 10 mm/s. It commands exactly 15 mm when
 * neither side is current-limited, disables the driver, and never starts full
 * homing.
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

    char message[640];
    snprintf(
        message, sizeof(message),
        "limit=%.3f negative_avg=%.3f negative_peak=%.3f "
        "negative_completion=%.3f negative_ms=%u negative_blocked=%d "
        "positive_avg=%.3f positive_peak=%.3f positive_completion=%.3f "
        "positive_ms=%u positive_blocked=%d "
        "direction=%d negative_timeout=%d positive_timeout=%d "
        "total_commanded_mm=15",
        diagnostics.adaptiveCurrentLimit,
        diagnostics.negativeAverageLoad, diagnostics.negativePeakLoad,
        diagnostics.negativeCompletionRatio, diagnostics.negativeElapsedMs,
        diagnostics.negativeHitHardLimit,
        diagnostics.positiveAverageLoad, diagnostics.positivePeakLoad,
        diagnostics.positiveCompletionRatio, diagnostics.positiveElapsedMs,
        diagnostics.positiveHitHardLimit, diagnostics.direction,
        diagnostics.negativeTimedOut, diagnostics.positiveTimedOut);
    Serial.printf("HOMING_PROBE_RESULT %s\n", message);
    Serial.flush();
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
