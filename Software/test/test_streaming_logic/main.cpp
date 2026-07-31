#include <unity.h>
#include "streaming_logic.h"

void setUp(void) {}
void tearDown(void) {}

// ─── calculateMaxStroke ───

void test_calculateMaxStroke_stroke_less_than_depth(void) {
    // stroke=50%, depth=80%, measured=10000 → min(50,80)=50% → 5000
    int32_t result = streaming_logic::calculateMaxStroke(50.0f, 80.0f, 10000.0f);
    TEST_ASSERT_EQUAL_INT32(5000, result);
}

void test_calculateMaxStroke_depth_less_than_stroke(void) {
    // stroke=80%, depth=50%, measured=10000 → min(80,50)=50% → 5000
    int32_t result = streaming_logic::calculateMaxStroke(80.0f, 50.0f, 10000.0f);
    TEST_ASSERT_EQUAL_INT32(5000, result);
}

void test_calculateMaxStroke_both_100(void) {
    // stroke=100%, depth=100%, measured=10000 → 10000
    int32_t result = streaming_logic::calculateMaxStroke(100.0f, 100.0f, 10000.0f);
    TEST_ASSERT_EQUAL_INT32(10000, result);
}

// ─── calculateDepthOffset ───

void test_calculateDepthOffset_full_depth(void) {
    // measured=10000, maxStroke=5000, depthPct=100 → (10000-5000)*1.0 = 5000
    int32_t result = streaming_logic::calculateDepthOffset(10000.0f, 5000, 100.0f);
    TEST_ASSERT_EQUAL_INT32(5000, result);
}

void test_calculateDepthOffset_zero_depth(void) {
    // depthPct=0 → 0
    int32_t result = streaming_logic::calculateDepthOffset(10000.0f, 5000, 0.0f);
    TEST_ASSERT_EQUAL_INT32(0, result);
}

// ─── scaleStreamPosition ───

void test_scaleStreamPosition_100_percent(void) {
    // posPercent=100 → -(1-1.0)*maxStroke - depth = -depth
    int32_t maxStroke = 5000;
    int32_t depth = 2000;
    int32_t result = streaming_logic::scaleStreamPosition(100, maxStroke, depth);
    TEST_ASSERT_EQUAL_INT32(-depth, result);
}

void test_scaleStreamPosition_0_percent(void) {
    // posPercent=0 → -(1-0)*maxStroke - depth = -maxStroke - depth
    int32_t maxStroke = 5000;
    int32_t depth = 2000;
    int32_t result = streaming_logic::scaleStreamPosition(0, maxStroke, depth);
    TEST_ASSERT_EQUAL_INT32(-maxStroke - depth, result);
}

void test_scaleStreamPosition_50_percent(void) {
    // posPercent=50 → -(1-0.5)*5000 - 2000 = -2500 - 2000 = -4500
    int32_t maxStroke = 5000;
    int32_t depth = 2000;
    int32_t result = streaming_logic::scaleStreamPosition(50, maxStroke, depth);
    TEST_ASSERT_EQUAL_INT32(-4500, result);
}

void test_clampStreamPosition_keeps_targets_inside_inner_play_range(void) {
    TEST_ASSERT_EQUAL_UINT8(10, streaming_logic::clampStreamPosition(0));
    TEST_ASSERT_EQUAL_UINT8(10, streaming_logic::clampStreamPosition(10));
    TEST_ASSERT_EQUAL_UINT8(50, streaming_logic::clampStreamPosition(50));
    TEST_ASSERT_EQUAL_UINT8(90, streaming_logic::clampStreamPosition(90));
    TEST_ASSERT_EQUAL_UINT8(90, streaming_logic::clampStreamPosition(100));
}

void test_streaming_speed_limit_is_bounded_and_percentage_scaled(void) {
    TEST_ASSERT_EQUAL_UINT32(
        0, streaming_logic::calculateStreamingSpeedLimit(-10.0f, 20.0f));
    TEST_ASSERT_EQUAL_UINT32(
        1600, streaming_logic::calculateStreamingSpeedLimit(40.0f, 20.0f));
    TEST_ASSERT_EQUAL_UINT32(
        4000, streaming_logic::calculateStreamingSpeedLimit(150.0f, 20.0f));
}

void test_streaming_acceleration_limit_preserves_tuning_scale(void) {
    TEST_ASSERT_EQUAL_UINT32(
        0, streaming_logic::calculateStreamingAccelerationLimit(
               0.0f, 1.0f, 20.0f));
    TEST_ASSERT_EQUAL_UINT32(
        12500, streaming_logic::calculateStreamingAccelerationLimit(
                   50.0f, 0.25f, 20.0f));
    TEST_ASSERT_EQUAL_UINT32(
        50000, streaming_logic::calculateStreamingAccelerationLimit(
                   50.0f, 1.0f, 20.0f));
}

// ─── planMotion ───

void test_planMotion_short_distance_plenty_of_time(void) {
    // Short move with ample time → valid triangular profile
    auto profile = streaming_logic::planMotion(
        0,       // currentPosition
        1000,    // targetPosition
        1.0f,    // timeSeconds
        50000,   // maxSpeed
        100000,  // maxAccel
        1        // stepsPerMm
    );
    TEST_ASSERT_GREATER_THAN(0u, profile.speed);
    TEST_ASSERT_GREATER_THAN(0u, profile.acceleration);
    TEST_ASSERT_EQUAL_INT32(1000, profile.distance);
    TEST_ASSERT_EQUAL_INT32(1000, profile.targetPosition);
}

void test_planMotion_distance_exceeds_max(void) {
    // Very large distance, small time → distance gets clamped
    auto profile = streaming_logic::planMotion(
        0,       // currentPosition
        100000,  // targetPosition (way too far)
        0.1f,    // timeSeconds (very short)
        5000,    // maxSpeed
        100000,  // maxAccel
        1        // stepsPerMm
    );
    // Distance should be clamped below the requested 100000
    TEST_ASSERT_LESS_THAN(100000, profile.distance);
    // Target should be adjusted
    TEST_ASSERT_LESS_THAN(100000, profile.targetPosition);
    TEST_ASSERT_GREATER_THAN(0u, profile.speed);
    TEST_ASSERT_GREATER_THAN(0u, profile.acceleration);
}

void test_planMotion_speed_clamped_to_max(void) {
    // Large distance, short time → speed would exceed maxSpeed, should be clamped
    uint32_t maxSpeed = 1000;
    auto profile = streaming_logic::planMotion(
        0,       // currentPosition
        500,     // targetPosition
        0.5f,    // timeSeconds
        maxSpeed,// maxSpeed
        100000,  // maxAccel
        1        // stepsPerMm
    );
    TEST_ASSERT_LESS_OR_EQUAL(maxSpeed, profile.speed);
}

void test_planMotion_accel_clamped_to_max(void) {
    uint32_t maxAccel = 500;
    auto profile = streaming_logic::planMotion(
        0,       // currentPosition
        100,     // targetPosition
        0.5f,    // timeSeconds
        50000,   // maxSpeed
        maxAccel,// maxAccel
        1        // stepsPerMm
    );
    TEST_ASSERT_LESS_OR_EQUAL(maxAccel, profile.acceleration);
}

void test_planMotion_very_short_time(void) {
    // Very short time (0.01s) → profile still valid
    auto profile = streaming_logic::planMotion(
        0,       // currentPosition
        5000,    // targetPosition
        0.01f,   // timeSeconds
        50000,   // maxSpeed
        100000,  // maxAccel
        1        // stepsPerMm
    );
    TEST_ASSERT_GREATER_THAN(0u, profile.speed);
    TEST_ASSERT_GREATER_THAN(0u, profile.acceleration);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_calculateMaxStroke_stroke_less_than_depth);
    RUN_TEST(test_calculateMaxStroke_depth_less_than_stroke);
    RUN_TEST(test_calculateMaxStroke_both_100);

    RUN_TEST(test_calculateDepthOffset_full_depth);
    RUN_TEST(test_calculateDepthOffset_zero_depth);

    RUN_TEST(test_scaleStreamPosition_100_percent);
    RUN_TEST(test_scaleStreamPosition_0_percent);
    RUN_TEST(test_scaleStreamPosition_50_percent);
    RUN_TEST(test_clampStreamPosition_keeps_targets_inside_inner_play_range);
    RUN_TEST(test_streaming_speed_limit_is_bounded_and_percentage_scaled);
    RUN_TEST(test_streaming_acceleration_limit_preserves_tuning_scale);

    RUN_TEST(test_planMotion_short_distance_plenty_of_time);
    RUN_TEST(test_planMotion_distance_exceeds_max);
    RUN_TEST(test_planMotion_speed_clamped_to_max);
    RUN_TEST(test_planMotion_accel_clamped_to_max);
    RUN_TEST(test_planMotion_very_short_time);

    return UNITY_END();
}
