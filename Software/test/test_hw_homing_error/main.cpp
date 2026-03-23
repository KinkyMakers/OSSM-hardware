/**
 * Hardware Homing Error Tests — require USB-connected OSSM
 *
 * Run:  pio test -e development -f test_hw_homing_error
 *
 * No motor movement — the motor driver is disabled immediately after
 * the homing task starts, so homing times out (~40 s) and the state
 * machine enters the error state.
 *
 * Total runtime: ~50 seconds (40 s homing timeout + assertions).
 *
 * Tests verify:
 *  1. When homing fails (timeout), the state machine enters error state
 *  2. The error state contains an appropriate error message
 *  3. The error → error.idle → error.help → restart flow works
 */

#include <Arduino.h>
#include <unity.h>

#include "constants/Config.h"
#include "constants/Pins.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/actions.h"
#include "ossm/state/calibration.h"
#include "ossm/state/error.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"
#include "services/stepper.h"
#include "services/tasks.h"

namespace sml = boost::sml;
using namespace sml;

// Homing timeout is 40 s in the firmware; wait a bit longer.
static constexpr uint32_t ERROR_WAIT_TIMEOUT_MS = 50000;

void setUp(void) {}
void tearDown(void) {
    ossmEmergencyStop();
}

// ─── Helpers ───────────────────────────────────────────────

/**
 * Block until the state machine reaches error.idle or timeout.
 */
static bool waitForErrorIdle(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!stateMachine->is("error.idle"_s)) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }
    return true;
}

// ─── Tests ─────────────────────────────────────────────────

/**
 * Q: Do we throw an error if homing fails?
 *
 * With the motor driver held disabled (enable pin HIGH), the homing
 * task cannot detect a current spike from hitting the end stop.
 * After 40 seconds it times out and fires Error{}, transitioning
 * the state machine to the error state.
 */
void test_homing_timeout_enters_error_state(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        waitForErrorIdle(ERROR_WAIT_TIMEOUT_MS),
        "State machine did not reach error.idle after homing timeout — "
        "expected homing to fail with motor disabled");
}

/**
 * The error state should contain a meaningful error message (not empty).
 */
void test_error_message_is_set(void) {
    TEST_ASSERT_GREATER_THAN_MESSAGE(
        0, errorState.message.length(),
        "errorState.message should not be empty after homing failure");
}

/**
 * The device should NOT be marked as homed after a homing failure.
 */
void test_not_homed_after_failure(void) {
    TEST_ASSERT_FALSE_MESSAGE(
        calibration.isHomed,
        "calibration.isHomed should be false after homing failure");
}

/**
 * From error.idle, a ButtonPress should transition to error.help.
 */
void test_error_idle_to_error_help(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("error.idle"_s),
        "Pre-condition: must be in error.idle");

    stateMachine->process_event(ButtonPress{});
    vTaskDelay(pdMS_TO_TICKS(200));

    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("error.help"_s),
        "ButtonPress from error.idle should transition to error.help");
}

/**
 * From error.help, a ButtonPress triggers restart (terminal state X).
 * We verify the SM accepts the event without crash.
 * Note: On real hardware this calls ESP.restart(), so the test ends here.
 *       If we reach the assertion, the transition was accepted.
 *
 * SKIP THIS TEST on real hardware — it would restart the device
 * and abort the test runner. Uncomment only for manual verification.
 */
// void test_error_help_to_restart(void) {
//     stateMachine->process_event(ButtonPress{});
//     // If we get here, ESP.restart() was called and the device rebooted.
//     // This test is inherently pass-by-observation.
// }

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    // Full hardware init
    initBoard();
    initDisplay();

    ossm = new OSSM();

    // Clear any stale error state
    errorState.message = "";

    // Init state machine — fires Done{} → idle → homing → homing.forward
    // The homing task launches on core 0 and calls stepper->enableOutputs().
    initStateMachine();

    // Let the homing task start on core 0, then kill it and disable the motor.
    // 10 ms is enough for the task to start (~0.1 mm of travel).
    // We must kill the task AND use forceStopAndNewPosition() to halt the
    // hardware timer ISR immediately — forceStop() only soft-decelerates
    // and leaves the ISR running with the original moveTo() target.
    vTaskDelay(pdMS_TO_TICKS(10));
    if (Tasks::runHomingTaskH != nullptr) {
        vTaskDelete(Tasks::runHomingTaskH);
        Tasks::runHomingTaskH = nullptr;
    }
    ossmEmergencyStop();

    UNITY_BEGIN();

    // This test blocks for ~40-45 seconds waiting for homing timeout
    RUN_TEST(test_homing_timeout_enters_error_state);
    RUN_TEST(test_error_message_is_set);
    RUN_TEST(test_not_homed_after_failure);
    RUN_TEST(test_error_idle_to_error_help);

    UNITY_END();
}

void loop() {}
