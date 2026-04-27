/**
 * Hardware State Machine Tests — require physically connected OSSM
 *
 * Run:  pio test -e hw_test -f test_hw_state_machine
 *
 * WARNING: The motor WILL move during homing (~25 s).
 *          Ensure the linear rail is clear and the device is powered.
 *
 * The test lets the full production homing sequence complete, then
 * verifies menu navigation transitions work correctly.
 *
 * Tests verify:
 *  1. The state machine reaches menu.idle after real homing
 *  2. Menu navigation transitions work correctly (help, wifi, update, pairing)
 *  3. Returning to menu from sub-states works correctly
 *
 * Note: Update and Pairing transitions trigger drawWiFi() when offline
 *       (the device won't be connected to WiFi during tests).  A cleanup
 *       delay between tests lets the WiFi portal task self-terminate
 *       before the next test starts a new one.
 */

#include <Arduino.h>
#include <unity.h>
#include "esp_log.h"

#include "constants/Config.h"
#include "constants/Menu.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/actions.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"

namespace sml = boost::sml;
using namespace sml;

void setUp(void) {}
void tearDown(void) {
    ossmEmergencyStop();
}

static constexpr uint32_t HOMING_TIMEOUT_MS = 30000;

// ─── Helpers ───────────────────────────────────────────────

/**
 * Block until the state machine reaches menu.idle or timeout.
 * This waits for the FULL transition chain: homing → menu → menu.idle.
 */
static bool waitForMenuIdle(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!stateMachine->is("menu.idle"_s)) {
        if (millis() - start > timeoutMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

/**
 * Set menu option, fire ButtonPress, give the SM a tick to settle.
 */
static void selectMenuOption(Menu option) {
    menuState.currentOption = option;
    stateMachine->process_event(ButtonPress{});
    vTaskDelay(pdMS_TO_TICKS(200));
}

/**
 * Navigate back to menu and wait for menu.idle.
 * Uses ButtonPress from most sub-states.
 */
static bool returnToMenu(uint32_t waitMs = 2000) {
    stateMachine->process_event(ButtonPress{});
    vTaskDelay(pdMS_TO_TICKS(200));

    uint32_t start = millis();
    while (!stateMachine->is("menu.idle"_s)) {
        if (millis() - start > waitMs) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return true;
}

/**
 * Return to menu and wait for any background tasks (e.g. WiFi portal)
 * to self-terminate before the next test starts.
 */
static bool returnToMenuWithCleanup(uint32_t cleanupMs = 2000) {
    if (!returnToMenu()) return false;
    vTaskDelay(pdMS_TO_TICKS(cleanupMs));
    return true;
}

// ─── Tests: State Machine Boot ─────────────────────────────

/**
 * After real homing completes, the state machine should be in menu.idle
 * and calibration data should be valid.
 */
void test_state_machine_reaches_menu(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "State machine should be in menu.idle after homing — "
        "check motor, current sensor, and power supply");
    TEST_ASSERT_TRUE_MESSAGE(
        calibration.isHomed,
        "calibration.isHomed should be true after reaching menu");
}

// ─── Tests: Menu Navigation ────────────────────────────────

/**
 * Selecting Help from menu.idle should transition to help.idle.
 */
void test_menu_navigate_to_help(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::Help);

    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("help.idle"_s),
        "Should transition to help.idle after selecting Help");

    TEST_ASSERT_TRUE_MESSAGE(
        returnToMenu(),
        "Should return to menu.idle from help.idle via ButtonPress");
}

/**
 * Selecting WiFiSetup from menu.idle should transition to wifi.idle.
 * Starts a WiFi config portal — cleanup delay lets the portal task exit.
 */
void test_menu_navigate_to_wifi(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::WiFiSetup);

    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("wifi.idle"_s),
        "Should transition to wifi.idle after selecting WiFiSetup");

    TEST_ASSERT_TRUE_MESSAGE(
        returnToMenuWithCleanup(),
        "Should return to menu.idle from wifi.idle via ButtonPress");
}

/**
 * Selecting Update from menu.idle should transition to an update state.
 * When offline (typical in tests), falls through to wifi.idle.
 * When online, would go to update.checking.
 * Starts a WiFi portal when offline — cleanup delay lets it exit.
 */
void test_menu_navigate_to_update(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::UpdateOSSM);

    bool inUpdate = stateMachine->is("update.checking"_s) ||
                    stateMachine->is("update.idle"_s);
    bool inWifi = stateMachine->is("wifi"_s) ||
                  stateMachine->is("wifi.idle"_s);

    TEST_ASSERT_TRUE_MESSAGE(
        inUpdate || inWifi,
        "Should be in update or wifi state after selecting UpdateOSSM");

    TEST_ASSERT_TRUE_MESSAGE(
        returnToMenuWithCleanup(),
        "Should return to menu.idle from update/wifi via ButtonPress");
}

/**
 * Selecting Pairing when offline should go to pairing.wifi flow.
 * Selecting Pairing when online should go to pairing flow (HTTP auth).
 * When offline (typical in tests), enters pairing.wifi which starts a
 * WiFi portal — cleanup delay lets it exit.
 */
void test_menu_navigate_to_pairing(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::Pairing);

    bool inPairing = stateMachine->is("pairing"_s) ||
                     stateMachine->is("pairing.idle"_s);
    bool inPairingWifi = stateMachine->is("pairing.wifi"_s) ||
                         stateMachine->is("pairing.wifi.idle"_s);

    TEST_ASSERT_TRUE_MESSAGE(
        inPairing || inPairingWifi,
        "Should be in pairing or pairing.wifi state after selecting Pairing");

    TEST_ASSERT_TRUE_MESSAGE(
        returnToMenuWithCleanup(),
        "Should return to menu.idle from pairing via ButtonPress");
}

/**
 * After navigating through multiple menu items, we should still be able
 * to return to menu.idle each time (state machine is stable).
 * Uses Help only — no side effects, no WiFi portal.
 */
void test_menu_round_trip_stability(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::Help);
    TEST_ASSERT_TRUE(returnToMenu());

    selectMenuOption(Menu::Help);
    TEST_ASSERT_TRUE(returnToMenu());

    selectMenuOption(Menu::Help);
    TEST_ASSERT_TRUE(returnToMenu());

    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Should be stable in menu.idle after multiple round-trips");
}

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    // Suppress verbose GPIO/ADC logs that slow down the current-sensing loop
    esp_log_level_set("gpio", ESP_LOG_WARN);

    initBoard();
    initDisplay();

    ossm = new OSSM();
    initStateMachine();  // fires Done{} → idle → homing (motor WILL move)
    waitForMenuIdle(HOMING_TIMEOUT_MS);

    UNITY_BEGIN();

    RUN_TEST(test_state_machine_reaches_menu);
    RUN_TEST(test_menu_navigate_to_help);
    RUN_TEST(test_menu_navigate_to_wifi);
    RUN_TEST(test_menu_navigate_to_update);
    RUN_TEST(test_menu_navigate_to_pairing);
    RUN_TEST(test_menu_round_trip_stability);

    UNITY_END();
}

void loop() {}
