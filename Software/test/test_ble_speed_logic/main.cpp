#include <unity.h>

#include <optional>

#include "ossm/state/ble.h"

void setUp() {}
void tearDown() {}

void test_stationary_knob_does_not_release_direct_ble_speed() {
    TEST_ASSERT_FALSE(speedKnobMoved(0.0f, 0.0f));
    TEST_ASSERT_FALSE(speedKnobMoved(42.0f, 43.5f));
}

void test_deliberate_knob_movement_releases_direct_ble_speed() {
    TEST_ASSERT_TRUE(speedKnobMoved(0.0f, 2.1f));
    TEST_ASSERT_TRUE(speedKnobMoved(80.0f, 77.0f));
}

void test_speed_without_ble_follows_physical_knob() {
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 37.0f, resolveBLESpeed(37.0f, std::nullopt, false));
}

void test_limited_ble_speed_scales_physical_knob() {
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 12.5f,
        resolveBLESpeed(50.0f, std::optional<float>(25.0f), true));
}

void test_direct_ble_speed_ignores_physical_knob() {
    const std::optional<float> commandedSpeed(25.0f);
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 25.0f, resolveBLESpeed(0.0f, commandedSpeed, false));
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 25.0f, resolveBLESpeed(100.0f, commandedSpeed, false));
}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_stationary_knob_does_not_release_direct_ble_speed);
    RUN_TEST(test_deliberate_knob_movement_releases_direct_ble_speed);
    RUN_TEST(test_speed_without_ble_follows_physical_knob);
    RUN_TEST(test_limited_ble_speed_scales_physical_knob);
    RUN_TEST(test_direct_ble_speed_ignores_physical_knob);
    return UNITY_END();
}
