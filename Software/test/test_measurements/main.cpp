#include "mock.h"
#include "unity.h"
#include "utils/analog.h"

// Test all readings zero
void test_allReadingsZero() {
    prepareAnalogReadData({0, 0, 0, 0, 0});  // 5 samples, all zero
    float result = getAnalogAveragePercent({0, 5});
    TEST_ASSERT_FLOAT_WITHIN(0.001, 0.0f, result);
}

// Test all readings maximum for 12-bit ADC
void test_allReadingsMaximum() {
    prepareAnalogReadData(
        {4096, 4096, 4096, 4096, 4096});  // 5 samples, all at max
    float result = getAnalogAveragePercent({0, 5});
    TEST_ASSERT_FLOAT_WITHIN(0.001, 100.0f, result);
}

// Test mixed readings
void test_mixedReadings() {
    prepareAnalogReadData({1024, 2048, 3072, 4096, 0});  // Mixed values
    float result = getAnalogAveragePercent({0, 5});
    TEST_ASSERT_FLOAT_WITHIN(
        0.001, 50.0f,
        result);  // Expected: (1024+2048+3072+4096+0)/5 / 4096 * 100
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_allReadingsZero);
    RUN_TEST(test_allReadingsMaximum);
    RUN_TEST(test_mixedReadings);
    return UNITY_END();
}

// WARNING!!! PLEASE REMOVE UNNECESSARY MAIN IMPLEMENTATIONS //

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void) { return runUnityTests(); }
