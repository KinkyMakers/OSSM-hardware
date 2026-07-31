#include <unity.h>

#include <cmath>
#include "streaming_stress_suite.h"

using namespace streaming_stress;

void setUp() {}
void tearDown() {}

void test_catalog_has_expected_parallel_suite_shape() {
    TEST_ASSERT_EQUAL_UINT32(8, kProfileCount);
    TEST_ASSERT_EQUAL_UINT32(24, kCoreCaseCount);
    TEST_ASSERT_EQUAL_UINT32(6, kFaultCount);
    TEST_ASSERT_EQUAL_UINT32(33, kCaseCount);

    for (size_t index = 0; index < kCaseCount; ++index) {
        Scenario scenario = scenarioAt(index);
        TEST_ASSERT_NOT_NULL(scenario.id);
        TEST_ASSERT_NOT_NULL(scenario.profile);
        TEST_ASSERT_LESS_OR_EQUAL_UINT8(100, scenario.profile->maximum);
        TEST_ASSERT_GREATER_OR_EQUAL_UINT8(scenario.profile->minimum,
                                           scenario.profile->maximum);
        TEST_ASSERT_GREATER_THAN_UINT8(0, scenario.profile->cadenceHz);
    }
}

void test_waveforms_are_deterministic_and_bounded() {
    for (size_t index = 0; index < kCaseCount; ++index) {
        Scenario scenario = scenarioAt(index);
        for (uint32_t timeMs = 0; timeMs <= kDurationMs; timeMs += 37) {
            float first = requestedPosition(scenario, timeMs);
            float second = requestedPosition(scenario, timeMs);
            TEST_ASSERT_FLOAT_WITHIN(0.00001f, first, second);
            TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(scenario.profile->minimum, first);
            TEST_ASSERT_LESS_OR_EQUAL_FLOAT(scenario.profile->maximum, first);
        }
    }
}

void test_random_path_contains_reversals_without_jumps() {
    Scenario scenario = scenarioAt(kProfileCount * 2);
    float previous = requestedPosition(scenario, 0);
    float previousDelta = 0;
    int reversals = 0;
    float maximumStep = 0;
    for (uint32_t timeMs = 20; timeMs <= kDurationMs; timeMs += 20) {
        float current = requestedPosition(scenario, timeMs);
        float delta = current - previous;
        maximumStep = std::max(maximumStep, std::fabs(delta));
        if ((delta > 0 && previousDelta < 0) || (delta < 0 && previousDelta > 0)) {
            ++reversals;
        }
        if (std::fabs(delta) > 0.0001f) previousDelta = delta;
        previous = current;
    }
    TEST_ASSERT_GREATER_THAN(3, reversals);
    TEST_ASSERT_LESS_THAN_FLOAT(8.0f, maximumStep);
}

void test_delivery_fault_helpers_are_bounded() {
    Scenario jitter = scenarioAt(kCoreCaseCount);
    for (uint32_t index = 0; index < 1000; ++index) {
        int16_t value = deterministicJitterMs(jitter, index);
        TEST_ASSERT_GREATER_OR_EQUAL_INT16(-20, value);
        TEST_ASSERT_LESS_OR_EQUAL_INT16(20, value);
    }
    TEST_ASSERT_TRUE(inDeliveryGap(scenarioAt(kCoreCaseCount + 1), 7900));
    TEST_ASSERT_FALSE(inDeliveryGap(scenarioAt(kCoreCaseCount + 1), 9000));
    TEST_ASSERT_EQUAL_UINT16(25, irregularIntervalMs(0));
    TEST_ASSERT_EQUAL_UINT16(200, irregularIntervalMs(4));
    TEST_ASSERT_EQUAL_UINT16(25, irregularIntervalMs(5));
}

void test_micro_macro_path_contains_small_and_large_smooth_moves() {
    Scenario scenario = scenarioAt(kCoreCaseCount + kFaultCount - 1);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(Delivery::MicroMacro),
                            static_cast<uint8_t>(scenario.delivery));
    float center = (scenario.profile->minimum + scenario.profile->maximum) / 2.0f;
    float amplitude = (scenario.profile->maximum - scenario.profile->minimum) / 2.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, center - amplitude * 0.08f,
                             requestedPosition(scenario, 600));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, center + amplitude * 0.90f,
                             requestedPosition(scenario, 800));
    float maximumStep = 0;
    float previous = requestedPosition(scenario, 0);
    for (uint32_t timeMs = 20; timeMs <= 2700; timeMs += 20) {
        float current = requestedPosition(scenario, timeMs);
        maximumStep = std::max(maximumStep, std::fabs(current - previous));
        previous = current;
    }
    TEST_ASSERT_LESS_THAN_FLOAT(8.0f, maximumStep);
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_catalog_has_expected_parallel_suite_shape);
    RUN_TEST(test_waveforms_are_deterministic_and_bounded);
    RUN_TEST(test_random_path_contains_reversals_without_jumps);
    RUN_TEST(test_delivery_fault_helpers_are_bounded);
    RUN_TEST(test_micro_macro_path_contains_small_and_large_smooth_moves);
    return UNITY_END();
}
