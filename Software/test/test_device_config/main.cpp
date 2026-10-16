#include <unity.h>
#include "device_config_logic.h"

using namespace device_config_logic;

void setUp(void) {}
void tearDown(void) {}

static constexpr float kAccel = 50000.0f;

// ─── Derived values ───

void test_defaults_match_previous_constants(void) {
    MachineConfig config;
    TEST_ASSERT_EQUAL_FLOAT(20.0f, stepsPerMm(config));
    TEST_ASSERT_EQUAL_FLOAT(20000.0f, maxStepRateHz(config));
    TEST_ASSERT_EQUAL_FLOAT(1000.0f, maxSpeedMmPerSecond(config));
}

void test_defaults_are_valid(void) {
    TEST_ASSERT_TRUE(validateGeometry(MachineConfig{}, kAccel) ==
                     GeometryError::None);
}

// ─── Step rate cap ───

void test_step_rate_at_cap_is_valid(void) {
    MachineConfig config;
    config.stepsPerRev = 4000.0f;
    config.maxRpm = 1500.0f;  // 4000 * 1500 / 60 = 100000
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) == GeometryError::None);
}

void test_step_rate_above_cap_is_rejected(void) {
    MachineConfig config;
    config.stepsPerRev = 4000.0f;
    config.maxRpm = 1501.0f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::StepRateTooHigh);
}

// ─── Steps per mm ───

void test_steps_per_mm_below_minimum_is_rejected(void) {
    MachineConfig config;
    config.stepsPerRev = 79.0f;  // 79 / 40 = 1.975
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::StepsPerMmTooLow);
}

void test_steps_per_mm_at_minimum_is_valid(void) {
    MachineConfig config;
    config.stepsPerRev = 80.0f;  // 80 / 40 = 2
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) == GeometryError::None);
}

void test_high_steps_per_mm_without_overflow_is_valid(void) {
    MachineConfig config;
    config.stepsPerRev = 51200.0f;  // 1280 steps/mm
    config.maxRpm = 100.0f;         // 85333 steps/s
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) == GeometryError::None);
}

void test_steps_per_mm_that_overflows_acceleration_is_rejected(void) {
    MachineConfig config;
    config.stepsPerRev = 2000000.0f;  // 50000 steps/mm; 50000 mm/s² overflows
    config.maxRpm = 1.0f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::WouldOverflow);
}

// ─── Field checks ───

void test_zero_field_is_rejected(void) {
    MachineConfig config;
    config.beltPitchMm = 0.0f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::NotPositive);
}

void test_nan_field_is_rejected(void) {
    MachineConfig config;
    config.maxRpm = NAN;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::NotPositive);
}

void test_stroke_at_minimum_is_rejected(void) {
    MachineConfig config;
    config.maxStrokeMm = kMinStrokeLengthMm;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::StrokeTooShort);
}

void test_long_stroke_is_valid(void) {
    MachineConfig config;
    config.maxStrokeMm = 2000.0f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) == GeometryError::None);
}

// ─── clampStepRateHz ───

void test_clamp_below_limit_is_unchanged(void) {
    TEST_ASSERT_EQUAL_UINT32(5000, clampStepRateHz(5000.0f, 20000.0f));
}

void test_clamp_to_machine_limit(void) {
    TEST_ASSERT_EQUAL_UINT32(20000, clampStepRateHz(250000.0f, 20000.0f));
}

void test_clamp_never_exceeds_global_cap(void) {
    TEST_ASSERT_EQUAL_UINT32(100000, clampStepRateHz(500000.0f, 400000.0f));
}

void test_clamp_zero_and_nan_to_one(void) {
    TEST_ASSERT_EQUAL_UINT32(1, clampStepRateHz(0.0f, 20000.0f));
    TEST_ASSERT_EQUAL_UINT32(1, clampStepRateHz(NAN, 20000.0f));
}

// ─── Live settings ───

void test_after_homing_position_range(void) {
    TEST_ASSERT_TRUE(isValidAfterHomingPosition(0.0f));
    TEST_ASSERT_TRUE(isValidAfterHomingPosition(0.5f));
    TEST_ASSERT_TRUE(isValidAfterHomingPosition(1.0f));
    TEST_ASSERT_FALSE(isValidAfterHomingPosition(-0.1f));
    TEST_ASSERT_FALSE(isValidAfterHomingPosition(1.1f));
    TEST_ASSERT_FALSE(isValidAfterHomingPosition(NAN));
}

void test_fractional_steps_per_rev_is_rejected(void) {
    MachineConfig config;
    config.stepsPerRev = 800.5f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::NotWholeNumber);
}

void test_fractional_pulley_teeth_is_rejected(void) {
    MachineConfig config;
    config.pulleyToothCount = 20.5f;
    TEST_ASSERT_TRUE(validateGeometry(config, kAccel) ==
                     GeometryError::NotWholeNumber);
}

void test_mqtt_publish_hz_range(void) {
    TEST_ASSERT_TRUE(isValidMqttPublishHz(1.0f));
    TEST_ASSERT_TRUE(isValidMqttPublishHz(100.0f));
    TEST_ASSERT_FALSE(isValidMqttPublishHz(0.5f));
    TEST_ASSERT_FALSE(isValidMqttPublishHz(101.0f));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_defaults_match_previous_constants);
    RUN_TEST(test_defaults_are_valid);

    RUN_TEST(test_step_rate_at_cap_is_valid);
    RUN_TEST(test_step_rate_above_cap_is_rejected);

    RUN_TEST(test_steps_per_mm_below_minimum_is_rejected);
    RUN_TEST(test_steps_per_mm_at_minimum_is_valid);
    RUN_TEST(test_high_steps_per_mm_without_overflow_is_valid);
    RUN_TEST(test_steps_per_mm_that_overflows_acceleration_is_rejected);

    RUN_TEST(test_zero_field_is_rejected);
    RUN_TEST(test_nan_field_is_rejected);
    RUN_TEST(test_stroke_at_minimum_is_rejected);
    RUN_TEST(test_long_stroke_is_valid);

    RUN_TEST(test_clamp_below_limit_is_unchanged);
    RUN_TEST(test_clamp_to_machine_limit);
    RUN_TEST(test_clamp_never_exceeds_global_cap);
    RUN_TEST(test_clamp_zero_and_nan_to_one);

    RUN_TEST(test_after_homing_position_range);
    RUN_TEST(test_mqtt_publish_hz_range);
    RUN_TEST(test_fractional_steps_per_rev_is_rejected);
    RUN_TEST(test_fractional_pulley_teeth_is_rejected);

    return UNITY_END();
}
