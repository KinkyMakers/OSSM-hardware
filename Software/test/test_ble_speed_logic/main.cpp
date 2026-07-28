#include <unity.h>

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

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_stationary_knob_does_not_release_direct_ble_speed);
    RUN_TEST(test_deliberate_knob_movement_releases_direct_ble_speed);
    return UNITY_END();
}
