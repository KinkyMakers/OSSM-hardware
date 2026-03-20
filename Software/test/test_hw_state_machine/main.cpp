/**
 * Hardware State Machine Tests — require USB-connected OSSM
 *
 * Run:  pio test -e hw_test -f test_hw_state_machine
 *
 * No motor movement — homing is bypassed by killing each spawned
 * homing task and manually advancing the state machine to menu.idle.
 *
 * Tests verify:
 *  1. The state machine reaches menu.idle after bypassed homing
 *  2. Menu navigation transitions work correctly for each menu option
 *  3. Returning to menu from sub-states works correctly
 */

#include <Arduino.h>
#include <unity.h>

#include "constants/Config.h"
#include "constants/Menu.h"
#include "constants/Pins.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"
#include "services/stepper.h"
#include "services/tasks.h"

namespace sml = boost::sml;
using namespace sml;

void setUp(void) {}
void tearDown(void) {
    // Safety: disable motor after each test
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
}

// ─── Helpers ───────────────────────────────────────────────

/**
 * Kill a freshly-spawned homing task and disable the motor.
 * Called after each state machine transition that triggers startHoming.
 */
static void killHomingTask() {
    vTaskDelay(pdMS_TO_TICKS(10));  // let task start on core 0
    if (Tasks::runHomingTaskH != nullptr) {
        vTaskDelete(Tasks::runHomingTaskH);
        Tasks::runHomingTaskH = nullptr;
    }
    stepper->forceStop();
    digitalWrite(Pins::Driver::motorEnablePin, HIGH);
}

/**
 * Bypass homing entirely: advance the state machine from
 * homing.forward → homing.backward → menu.idle, killing each
 * homing task before the motor moves significantly (~0.1 mm).
 */
static void skipHomingToMenu() {
    // initStateMachine() fires Done{} → idle → homing → homing.forward
    // which spawns a homing task on core 0.
    initStateMachine();
    killHomingTask();

    // Fake calibration so the isStrokeTooShort guard passes.
    calibration.measuredStrokeSteps = 5000;

    // Advance: homing.forward → homing.backward (spawns another task).
    stateMachine->process_event(Done{});
    killHomingTask();

    // Advance: homing.backward → menu → menu.idle
    // (isFirstHomed guard returns true on first call)
    stateMachine->process_event(Done{});
    vTaskDelay(pdMS_TO_TICKS(10));  // let menu entry action settle
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

// ─── Tests: State Machine Boot ─────────────────────────────

/**
 * After bypassed homing, the state machine should be in menu.idle.
 */
void test_state_machine_reaches_menu(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "State machine should be in menu.idle after homing bypass");
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
        returnToMenu(),
        "Should return to menu.idle from wifi.idle via ButtonPress");
}

/**
 * Selecting Update from menu.idle should transition to an update state.
 * If offline, it falls through to wifi. If online, it goes to update.checking.
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
        returnToMenu(),
        "Should return to menu.idle from update/wifi via ButtonPress");
}

/**
 * Selecting Pairing when offline should go to pairing.wifi flow.
 * Selecting Pairing when online should go to pairing flow.
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
        returnToMenu(),
        "Should return to menu.idle from pairing via ButtonPress");
}

/**
 * After navigating through multiple menu items, we should still be able
 * to return to menu.idle each time (state machine is stable).
 */
void test_menu_round_trip_stability(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        stateMachine->is("menu.idle"_s),
        "Pre-condition: must be in menu.idle");

    selectMenuOption(Menu::Help);
    TEST_ASSERT_TRUE(returnToMenu());

    selectMenuOption(Menu::WiFiSetup);
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

    initBoard();
    initDisplay();

    ossm = new OSSM();
    skipHomingToMenu();  // no motor movement

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
