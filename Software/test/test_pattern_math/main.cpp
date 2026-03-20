// ┌──────────────────────────────────────────────────────────────────────────┐
// │ PATTERN MATH — UNIT TESTS                                              │
// │                                                                        │
// │ Tests for lib/StrokeEngine/src/PatternMath.h:                          │
// │   fscale()              — Logarithmic range mapping                    │
// │   fmap()                — Linear float mapping                         │
// │   mapSensationToFactor() — Sensation (-100..+100) to multiplier        │
// │                                                                        │
// │ All functions are pure math with no hardware dependencies.             │
// └──────────────────────────────────────────────────────────────────────────┘

#include <ArduinoFake.h>
#include <unity.h>

#include "PatternMath.h"

void setUp(void) {}
void tearDown(void) {}

// ─── fscale() ────────────────────────────────────────────────────────────

void test_fscale_linear_mapping() {
    // curve = 0 should produce linear interpolation
    // Midpoint input should map to midpoint output
    float result = fscale(0.0f, 100.0f, 0.0f, 200.0f, 50.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 100.0f, result);
}

void test_fscale_input_at_min_returns_newBegin() {
    float result = fscale(0.0f, 100.0f, 10.0f, 50.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 10.0f, result);
}

void test_fscale_input_at_max_returns_newEnd() {
    float result = fscale(0.0f, 100.0f, 10.0f, 50.0f, 100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, result);
}

void test_fscale_positive_curve_favors_high_end() {
    // Positive curve gives more weight to the high end of output,
    // so midpoint input maps ABOVE linear midpoint
    float linear = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, 0.0f);
    float curved = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, 5.0f);
    TEST_ASSERT_TRUE(curved > linear);
}

void test_fscale_negative_curve_favors_low_end() {
    // Negative curve gives more weight to the low end of output,
    // so midpoint input maps BELOW linear midpoint
    float linear = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, 0.0f);
    float curved = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, -5.0f);
    TEST_ASSERT_TRUE(curved < linear);
}

void test_fscale_clamps_input_below_min() {
    // Input below originalMin should be treated as originalMin
    float atMin = fscale(0.0f, 100.0f, 0.0f, 200.0f, 0.0f, 0.0f);
    float belowMin = fscale(0.0f, 100.0f, 0.0f, 200.0f, -50.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, atMin, belowMin);
}

void test_fscale_clamps_input_above_max() {
    // Input above originalMax should be treated as originalMax
    float atMax = fscale(0.0f, 100.0f, 0.0f, 200.0f, 100.0f, 0.0f);
    float aboveMax = fscale(0.0f, 100.0f, 0.0f, 200.0f, 150.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, atMax, aboveMax);
}

void test_fscale_inverted_output_range() {
    // newBegin > newEnd should invert the mapping
    float result = fscale(0.0f, 100.0f, 200.0f, 0.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, result);

    float resultMax = fscale(0.0f, 100.0f, 200.0f, 0.0f, 100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, resultMax);
}

void test_fscale_originalMin_greater_than_max_returns_zero() {
    float result = fscale(100.0f, 0.0f, 0.0f, 200.0f, 50.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, result);
}

void test_fscale_clamps_curve_to_range() {
    // Curve > 10 should be treated as 10, curve < -10 as -10
    // Just verify it doesn't crash and returns valid values
    float result1 = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, 20.0f);
    float result2 = fscale(0.0f, 100.0f, 0.0f, 100.0f, 50.0f, -20.0f);
    TEST_ASSERT_TRUE(result1 >= 0.0f && result1 <= 100.0f);
    TEST_ASSERT_TRUE(result2 >= 0.0f && result2 <= 100.0f);
}

// ─── fmap() ──────────────────────────────────────────────────────────────

void test_fmap_standard_mapping() {
    // Map 50 from [0,100] to [0,200] → 100
    float result = fmap(50.0f, 0.0f, 100.0f, 0.0f, 200.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, result);
}

void test_fmap_inverted_output() {
    // Map 0 from [0,100] to [200,0] → 200
    float result = fmap(0.0f, 0.0f, 100.0f, 200.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, result);
}

void test_fmap_identity() {
    // Same input and output range → same value
    float result = fmap(42.0f, 0.0f, 100.0f, 0.0f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 42.0f, result);
}

// ─── mapSensationToFactor() ──────────────────────────────────────────────

void test_sensation_zero_returns_one() {
    float result = mapSensationToFactor(5.0f, 0.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, result);
}

void test_sensation_positive_returns_above_one() {
    float result = mapSensationToFactor(5.0f, 50.0f, 0.0f);
    TEST_ASSERT_TRUE(result > 1.0f);
}

void test_sensation_negative_returns_below_one() {
    float result = mapSensationToFactor(5.0f, -50.0f, 0.0f);
    TEST_ASSERT_TRUE(result < 1.0f);
    TEST_ASSERT_TRUE(result > 0.0f);
}

void test_sensation_max_returns_maximum_factor() {
    float result = mapSensationToFactor(5.0f, 100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, result);
}

void test_sensation_min_returns_inverse_of_max() {
    float result = mapSensationToFactor(5.0f, -100.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f / 5.0f, result);
}

void test_sensation_negative_is_inverse_of_positive() {
    float pos = mapSensationToFactor(5.0f, 60.0f, 0.0f);
    float neg = mapSensationToFactor(5.0f, -60.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, pos * neg);
}

void test_sensation_clamped_beyond_range() {
    // Values beyond ±100 should be clamped
    float at100 = mapSensationToFactor(5.0f, 100.0f, 0.0f);
    float at150 = mapSensationToFactor(5.0f, 150.0f, 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, at100, at150);
}

// ─── Runner ──────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    // fscale
    RUN_TEST(test_fscale_linear_mapping);
    RUN_TEST(test_fscale_input_at_min_returns_newBegin);
    RUN_TEST(test_fscale_input_at_max_returns_newEnd);
    RUN_TEST(test_fscale_positive_curve_favors_high_end);
    RUN_TEST(test_fscale_negative_curve_favors_low_end);
    RUN_TEST(test_fscale_clamps_input_below_min);
    RUN_TEST(test_fscale_clamps_input_above_max);
    RUN_TEST(test_fscale_inverted_output_range);
    RUN_TEST(test_fscale_originalMin_greater_than_max_returns_zero);
    RUN_TEST(test_fscale_clamps_curve_to_range);

    // fmap
    RUN_TEST(test_fmap_standard_mapping);
    RUN_TEST(test_fmap_inverted_output);
    RUN_TEST(test_fmap_identity);

    // mapSensationToFactor
    RUN_TEST(test_sensation_zero_returns_one);
    RUN_TEST(test_sensation_positive_returns_above_one);
    RUN_TEST(test_sensation_negative_returns_below_one);
    RUN_TEST(test_sensation_max_returns_maximum_factor);
    RUN_TEST(test_sensation_min_returns_inverse_of_max);
    RUN_TEST(test_sensation_negative_is_inverse_of_positive);
    RUN_TEST(test_sensation_clamped_beyond_range);

    return UNITY_END();
}
