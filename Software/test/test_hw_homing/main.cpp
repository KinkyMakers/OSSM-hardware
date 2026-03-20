/**
 * Hardware Homing Tests — require physically connected OSSM with working motor
 *
 * Run:  pio test -e development -f test_hw_homing
 *
 * WARNING: The motor WILL move during these tests.
 *          Ensure the linear rail is clear and the device is powered.
 *
 * The test boots the full state machine (initStateMachine() fires Done{}
 * internally, same as normal startup) and waits for homing to complete.
 */

#include <Arduino.h>
#include <unity.h>

#include "constants/Config.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/calibration.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"

static constexpr uint32_t HOMING_TIMEOUT_MS = 30000;

void setUp(void) {}
void tearDown(void) {
    // Safety: disable motor after each test (especially on failure/timeout)
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
}

// ─── Helpers ────────────────────────────────────────────────

static bool waitForHomed(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!calibration.isHomed) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

// ─── Tests ──────────────────────────────────────────────────

void test_homing_completes_within_timeout(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        waitForHomed(HOMING_TIMEOUT_MS),
        "Homing did not complete within 30 s — check motor, "
        "current sensor, and power supply");
}

void test_measured_stroke_reasonable(void) {
    TEST_ASSERT_GREATER_THAN_MESSAGE(
        Config::Driver::minStrokeLengthMm,
        calibration.measuredStrokeSteps,
        "Measured stroke is below minimum — homing may have failed or "
        "stroke is physically too short");
}

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    // Full hardware init (same as normal boot minus BLE/WiFi/MQTT)
    initBoard();
    initDisplay();

    ossm = new OSSM();
    initStateMachine();  // fires Done{} internally → idle → homing

    UNITY_BEGIN();

    // Blocks until homing completes or times out
    RUN_TEST(test_homing_completes_within_timeout);
    // Only meaningful if the first test passed
    RUN_TEST(test_measured_stroke_reasonable);

    UNITY_END();
}

void loop() {}
