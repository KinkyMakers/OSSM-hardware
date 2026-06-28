#include <unity.h>
#include "guard_logic.h"

void setUp(void) {}
void tearDown(void) {}

// ─── isPreflightSafeLogic ───

void test_isPreflightSafe_knob_near_zero(void) {
    // potReading=0.5, deadZone=2.0 → true (safe, knob near zero)
    TEST_ASSERT_TRUE(guard_logic::isPreflightSafeLogic(0.5f, 2.0f));
}

void test_isPreflightSafe_knob_too_high(void) {
    // potReading=5.0, deadZone=2.0 → false (not safe)
    TEST_ASSERT_FALSE(guard_logic::isPreflightSafeLogic(5.0f, 2.0f));
}

void test_isPreflightSafe_knob_at_deadzone(void) {
    // potReading=2.0, deadZone=2.0 → false (not strictly less)
    TEST_ASSERT_FALSE(guard_logic::isPreflightSafeLogic(2.0f, 2.0f));
}

// ─── isNotHomedLogic ───

void test_isNotHomed_when_homed(void) {
    // isHomed=true → false
    TEST_ASSERT_FALSE(guard_logic::isNotHomedLogic(true));
}

void test_isNotHomed_when_not_homed(void) {
    // isHomed=false → true
    TEST_ASSERT_TRUE(guard_logic::isNotHomedLogic(false));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_isPreflightSafe_knob_near_zero);
    RUN_TEST(test_isPreflightSafe_knob_too_high);
    RUN_TEST(test_isPreflightSafe_knob_at_deadzone);

    RUN_TEST(test_isNotHomed_when_homed);
    RUN_TEST(test_isNotHomed_when_not_homed);

    return UNITY_END();
}
