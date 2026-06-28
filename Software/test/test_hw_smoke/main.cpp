/**
 * Hardware Smoke Tests — require USB-connected OSSM
 *
 * Run:  pio test -e development -f test_hw_smoke
 *
 * These tests verify basic peripheral initialisation on real hardware.
 * No motor motion is commanded; the enable pin is toggled briefly.
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <unity.h>

#include "constants/Config.h"
#include "constants/Pins.h"
#include "ossm/state/actions.h"
#include "services/board.h"

void setUp(void) {}
void tearDown(void) {
    if (stepper != nullptr) {
        ossmEmergencyStop();
    }
}

// ─── Stepper ────────────────────────────────────────────────

void test_stepper_init(void) {
    // initBoard() → initStepper() already initialised the global stepper
    // via the singleton stepperEngine. Just verify it succeeded.
    TEST_ASSERT_NOT_NULL_MESSAGE(
        stepper,
        "Global stepper is nullptr after initBoard() — check step pin wiring");
}

// ─── Display ────────────────────────────────────────────────

void test_display_init(void) {
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C disp(U8G2_R0,
                                               Pins::Display::oledReset,
                                               Pins::Remote::displayClock,
                                               Pins::Remote::displayData);
    disp.begin();
    disp.clearBuffer();
    disp.setFont(u8g2_font_6x10_tf);
    disp.drawStr(0, 20, "HW TEST");
    disp.sendBuffer();

    // If we reach this line, no I2C hang or crash occurred.
    TEST_ASSERT_TRUE(true);
}

// ─── ADC ────────────────────────────────────────────────────

void test_adc_reading(void) {
    pinMode(Pins::Remote::speedPotPin, INPUT);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    int value = analogRead(Pins::Remote::speedPotPin);

    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(
        0, value, "ADC reading below 0 — unexpected");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(
        4095, value, "ADC reading above 4095 — unexpected for 12-bit");
}

// ─── Motor Enable Toggle ────────────────────────────────────

void test_motor_enable_toggle(void) {
    // Verify we can toggle the enable pin and read back expected state.
    // The global stepper must be initialised so the driver is in a
    // known-good state (no orphaned ISR pulses).
    TEST_ASSERT_NOT_NULL_MESSAGE(
        stepper, "Cannot test enable toggle — stepper not initialised");

    digitalWrite(Pins::Driver::motorEnablePin, HIGH);  // disable
    delay(10);
    TEST_ASSERT_EQUAL_MESSAGE(
        HIGH, digitalRead(Pins::Driver::motorEnablePin),
        "Enable pin should read HIGH (disabled)");

    // Leave motor disabled — tearDown() will enforce this too.
}

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    // Full GPIO + stepper init via the global singleton.
    // This configures all pins and runs the stepper safety sequence
    // (disable 600ms → enable 100ms) so the motor is in a known state.
    initBoard();

    UNITY_BEGIN();

    RUN_TEST(test_stepper_init);
    RUN_TEST(test_display_init);
    RUN_TEST(test_adc_reading);
    RUN_TEST(test_motor_enable_toggle);

    UNITY_END();
}

void loop() {}
