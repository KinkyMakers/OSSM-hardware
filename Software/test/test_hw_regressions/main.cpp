/**
 * Hardware Regression Tests — shared-stepper state across mode switches
 *
 * Run:  pio test -e hw_test -f test_hw_regressions
 *
 * WARNING: The motor WILL move: boot homing (~10 s) plus one short (~32 mm)
 *          positioning move in test 1. Keep the rail clear, no attachment,
 *          and the SPEED KNOB AT ZERO (preflight requires it, and it keeps
 *          both operation modes motionless during the tests).
 *
 * These tests pin down three real field bugs. Each is written so that it
 * FAILS on the pre-fix firmware (triggering/detecting the bug from counter
 * or config state — no crash required) and PASSES on the fixed firmware:
 *
 *  1. StrokeEngine re-entry origin drift
 *     Trigger: enter StrokeEngine → exit via the BLE go:menu path
 *     (ReturnToMenu: no re-home, isHomed stays true) → re-enter.
 *     Broken firmware re-bases the origin off the parked position in
 *     thisIsHome() (counter := abs(parked) − 120), shifting the machine's
 *     working window every entry until it reaches the physical end stop.
 *     Fixed firmware preserves the counter on re-entry (adopts the origin
 *     only once per physical home).
 *     Assert: counter after re-entry == parked counter (±40 steps).
 *
 *  2. SimplePenetration inherits StrokeEngine's inverted DIR polarity
 *     Trigger: StrokeEngine session → go:menu → SimplePenetration (no
 *     re-home in between). StrokeEngine::begin() flips the SHARED stepper
 *     direction polarity (directionPinHighCountsUp() == true) and only a
 *     homing pass resets it. Broken SimplePenetration never sets its own
 *     polarity, so it runs physically REVERSED — every stroke drives into
 *     the back plate. Fixed SimplePenetration sets the polarity explicitly
 *     at task entry.
 *     Assert: directionPinHighCountsUp() == false once SimplePenetration
 *     is running (read with the motor stationary — knob at zero).
 *
 *  3. Cross-mode switches leave the counter in the previous mode's frame
 *     Trigger: StrokeEngine parked off-home → go:menu → SimplePenetration
 *     (and the mirror direction). The two frames are exact mirror images
 *     (native: 0 at home rest, extension negative; StrokeEngine: home rest
 *     at −keepout, extension positive). Broken firmware hands the next mode
 *     the raw previous-frame counter, displacing its whole stroke window
 *     (worst case past a physical stop). Fixed firmware translates the
 *     counter exactly at every mode entry (stepperTranslateFrame).
 *     Assert: after parking at StrokeEngine counter 530 (= 650 steps out
 *     from home rest), SimplePenetration reads −650 for the same physical
 *     spot, and a StrokeEngine re-entry reads 530 again.
 */

#include <Arduino.h>
#include <unity.h>
#include "esp_log.h"

#include "constants/Menu.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/actions.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"
#include "services/stepper.h"

namespace sml = boost::sml;
using namespace sml;

void setUp(void) {}
void tearDown(void) {
    ossmEmergencyStop();
}

static constexpr uint32_t HOMING_TIMEOUT_MS = 30000;

// Steps tolerance for counter asserts. The buggy re-base shifts by exactly
// 120 steps (6 mm), so ±40 cleanly separates "preserved" from "re-based".
static constexpr int32_t TOL_STEPS = 40;

// Expected origin right after a fresh-home StrokeEngine entry:
// 0 − keepoutBoundary(6 mm) × stepsPerMm(20) = −120.
static constexpr int32_t FRESH_ORIGIN = -120;

// ─── Helpers ───────────────────────────────────────────────

static bool waitMenuIdle(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!stateMachine->is("menu.idle"_s)) {
        if (millis() - start > timeoutMs) return false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

static bool waitStrokeEngineIdle(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!stateMachine->is("strokeEngine.idle"_s)) {
        if (millis() - start > timeoutMs) return false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return true;
}

static bool waitSimplePenIdle(uint32_t timeoutMs) {
    uint32_t start = millis();
    while (!stateMachine->is("simplePenetration.idle"_s)) {
        if (millis() - start > timeoutMs) return false;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    return true;
}

static void selectMenuOption(Menu option) {
    menuState.currentOption = option;
    stateMachine->process_event(ButtonPress{});
    vTaskDelay(pdMS_TO_TICKS(200));
}

/** Exit the current mode via the BLE go:menu path (ReturnToMenu). This is
 *  the path a BLE disconnect / dashboard uses: emergencyStop only, NO
 *  setNotHomed, so no physical re-home happens on the next entry. */
static bool bleReturnToMenu() {
    stateMachine->process_event(ReturnToMenu{});
    return waitMenuIdle(3000);
}

// ─── Test 1: StrokeEngine re-entry origin drift ────────────

void test_strokeengine_reentry_preserves_position(void) {
    TEST_ASSERT_TRUE_MESSAGE(stateMachine->is("menu.idle"_s),
                             "Pre-condition: must be in menu.idle");
    TEST_ASSERT_TRUE_MESSAGE(calibration.isHomed,
                             "Pre-condition: must be homed");

    // Entry 1 — fresh home: the origin must be adopted exactly once, at -120.
    selectMenuOption(Menu::StrokeEngine);
    TEST_ASSERT_TRUE_MESSAGE(
        waitStrokeEngineIdle(3000),
        "StrokeEngine did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(900));  // let the task run begin()+thisIsHome()
    int32_t origin = stepper->getCurrentPosition();
    TEST_ASSERT_INT32_WITHIN_MESSAGE(
        TOL_STEPS, FRESH_ORIGIN, origin,
        "Fresh-home StrokeEngine entry should adopt origin -120");

    // Park the carriage off-home with a short blocking move inside the
    // engine's safe window [0, maxStep] (~32 mm; window is >=38 mm even at
    // the minimum calibrated stroke). The engine task is idle at speed 0.
    stepper->setSpeedInHz(1000);
    stepper->setAcceleration(20000);
    stepper->moveTo(origin + 650, true);
    int32_t parked = stepper->getCurrentPosition();

    // Exit via the BLE go:menu path: no re-home, isHomed stays true.
    TEST_ASSERT_TRUE_MESSAGE(bleReturnToMenu(),
                             "ReturnToMenu should land in menu.idle");
    TEST_ASSERT_TRUE_MESSAGE(calibration.isHomed,
                             "BLE exit must keep isHomed true (no re-home)");

    // Entry 2 — re-entry without a physical re-home. The counter is already
    // valid; it must be PRESERVED. Broken firmware re-bases the origin:
    // counter := abs(parked) - 120, i.e. a 120-step shift on every entry.
    selectMenuOption(Menu::StrokeEngine);
    TEST_ASSERT_TRUE_MESSAGE(
        waitStrokeEngineIdle(3000),
        "StrokeEngine re-entry did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(900));
    int32_t after = stepper->getCurrentPosition();
    TEST_ASSERT_INT32_WITHIN_MESSAGE(
        TOL_STEPS, parked, after,
        "ORIGIN DRIFT: re-entry re-based the origin off the parked position "
        "(expected the counter preserved; broken firmware yields parked-120)");

    TEST_ASSERT_TRUE(bleReturnToMenu());
}

// ─── Test 2: SimplePen inherits inverted DIR polarity ──────

void test_simplepen_owns_direction_after_strokeengine(void) {
    // Self-recover to menu.idle: when a prior test fails, Unity longjmps out
    // at the failing assert and that test's trailing cleanup never runs,
    // leaving the machine mid-mode. Recover so this test still exercises and
    // reports ITS OWN bug instead of dying on a stale pre-condition.
    if (!stateMachine->is("menu.idle"_s)) {
        stateMachine->process_event(ReturnToMenu{});
        waitMenuIdle(3000);
    }
    TEST_ASSERT_TRUE_MESSAGE(stateMachine->is("menu.idle"_s),
                             "Pre-condition: must be in menu.idle");
    TEST_ASSERT_TRUE_MESSAGE(calibration.isHomed,
                             "Pre-condition: must be homed");

    // StrokeEngine session: begin() flips the SHARED direction polarity to
    // inverted. Knob at zero -> the engine idles, no motion.
    selectMenuOption(Menu::StrokeEngine);
    TEST_ASSERT_TRUE_MESSAGE(
        waitStrokeEngineIdle(3000),
        "StrokeEngine did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(900));
    TEST_ASSERT_TRUE_MESSAGE(
        stepper->directionPinHighCountsUp(),
        "Sanity: a StrokeEngine session should leave the shared DIR polarity "
        "inverted (this is the state the next mode inherits)");

    // BLE go:menu exit (no re-home), then enter SimplePenetration.
    TEST_ASSERT_TRUE_MESSAGE(bleReturnToMenu(),
                             "ReturnToMenu should land in menu.idle");
    selectMenuOption(Menu::SimplePenetration);
    TEST_ASSERT_TRUE_MESSAGE(
        waitSimplePenIdle(3000),
        "SimplePenetration did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(600));  // let the task's entry code run

    // SimplePenetration's targets assume the homing frame's NORMAL polarity
    // (directionPinHighCountsUp == false). Broken firmware inherits
    // StrokeEngine's inverted latch, so every stroke runs physically
    // REVERSED - the back-plate slam. Read the live latch, motor stationary:
    TEST_ASSERT_FALSE_MESSAGE(
        stepper->directionPinHighCountsUp(),
        "DIR INVERSION: SimplePenetration is running on StrokeEngine's "
        "inverted direction polarity - its motion would be physically "
        "reversed (drives into the back plate)");

    TEST_ASSERT_TRUE(bleReturnToMenu());
}

// ─── Test 3: cross-mode counter frame translation ──────────

void test_cross_mode_frame_translation(void) {
    // Self-recover to menu.idle (see test 2).
    if (!stateMachine->is("menu.idle"_s)) {
        stateMachine->process_event(ReturnToMenu{});
        waitMenuIdle(3000);
    }
    TEST_ASSERT_TRUE_MESSAGE(stateMachine->is("menu.idle"_s),
                             "Pre-condition: must be in menu.idle");
    TEST_ASSERT_TRUE_MESSAGE(calibration.isHomed,
                             "Pre-condition: must be homed");

    // StrokeEngine session; park deterministically: first at the home rest
    // (-120 in the StrokeEngine frame), then 650 steps (32.5 mm) out. Safe
    // on any rig (minimum calibrated stroke is 50 mm).
    selectMenuOption(Menu::StrokeEngine);
    TEST_ASSERT_TRUE_MESSAGE(
        waitStrokeEngineIdle(3000),
        "StrokeEngine did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(900));
    stepper->setSpeedInHz(1000);
    stepper->setAcceleration(20000);
    stepper->moveTo(-120, true);  // physical home rest
    stepper->moveTo(530, true);   // 650 steps out; StrokeEngine frame
    TEST_ASSERT_INT32_WITHIN_MESSAGE(4, 530, stepper->getCurrentPosition(),
                                     "Setup: park move did not complete");

    // BLE exit -> SimplePenetration. Its targets are native-frame absolutes,
    // so the SAME physical spot must read -(530 + 120) = -650 here. Broken
    // firmware keeps the StrokeEngine-frame value (530): the whole stroke
    // window would be displaced ~59 mm toward the FRONT stop.
    TEST_ASSERT_TRUE_MESSAGE(bleReturnToMenu(),
                             "ReturnToMenu should land in menu.idle");
    selectMenuOption(Menu::SimplePenetration);
    TEST_ASSERT_TRUE_MESSAGE(
        waitSimplePenIdle(3000),
        "SimplePenetration did not reach idle - is the speed knob at ZERO?");
    vTaskDelay(pdMS_TO_TICKS(600));
    TEST_ASSERT_INT32_WITHIN_MESSAGE(
        40, -650, stepper->getCurrentPosition(),
        "FRAME MISMATCH: SimplePenetration inherited a StrokeEngine-frame "
        "counter - its stroke window is displaced toward the front stop");

    // And the mirror direction: re-entering StrokeEngine (no re-home) must
    // translate the native counter back: -(-650 + 120) = 530.
    TEST_ASSERT_TRUE_MESSAGE(bleReturnToMenu(),
                             "ReturnToMenu should land in menu.idle");
    selectMenuOption(Menu::StrokeEngine);
    TEST_ASSERT_TRUE_MESSAGE(waitStrokeEngineIdle(3000),
                             "StrokeEngine re-entry did not reach idle");
    vTaskDelay(pdMS_TO_TICKS(900));
    TEST_ASSERT_INT32_WITHIN_MESSAGE(
        40, 530, stepper->getCurrentPosition(),
        "FRAME MISMATCH: StrokeEngine inherited a native-frame counter - "
        "its pattern window is displaced");

    TEST_ASSERT_TRUE(bleReturnToMenu());
}

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    esp_log_level_set("gpio", ESP_LOG_WARN);

    initBoard();
    initDisplay();

    ossm = new OSSM();
    initStateMachine();  // fires Done{} -> idle -> homing (motor WILL move)
    waitMenuIdle(HOMING_TIMEOUT_MS);

    UNITY_BEGIN();

    RUN_TEST(test_strokeengine_reentry_preserves_position);
    RUN_TEST(test_simplepen_owns_direction_after_strokeengine);
    RUN_TEST(test_cross_mode_frame_translation);

    UNITY_END();
}

void loop() {}
