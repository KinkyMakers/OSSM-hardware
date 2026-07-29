#include <unity.h>
#include "homing_logic.h"

void setUp(void) {}
void tearDown(void) {}

// ─── isCurrentOverLimit ───

void test_isCurrentOverLimit_above_threshold(void) {
    // reading=5.0, offset=1.0, threshold=3.0 → (5-1)=4 > 3 → true
    TEST_ASSERT_TRUE(homing_logic::isCurrentOverLimit(5.0f, 1.0f, 3.0f));
}

void test_isCurrentOverLimit_below_threshold(void) {
    // reading=3.0, offset=1.0, threshold=3.0 → (3-1)=2 > 3 → false
    TEST_ASSERT_FALSE(homing_logic::isCurrentOverLimit(3.0f, 1.0f, 3.0f));
}

void test_isCurrentOverLimit_exactly_at_threshold(void) {
    // reading=4.5, offset=1.0, threshold=3.5 → (4.5-1)=3.5 > 3.5 → false (not strictly greater)
    TEST_ASSERT_FALSE(homing_logic::isCurrentOverLimit(4.5f, 1.0f, 3.5f));
}

void test_probe_escapes_negative_hard_stop(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Positive),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            7.0f, 1.0f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_escapes_positive_hard_stop(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Negative),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            1.0f, 7.0f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_selects_lower_current_direction(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Positive),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            3.0f, 1.0f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_selects_measured_lower_current_direction(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Negative),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            0.138f, 0.267f, 6.0f, 0.05f, 0.05f)));
}

void test_probe_rejects_ambiguous_direction(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Unsafe),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            1.0f, 1.2f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_rejects_both_directions_blocked(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Unsafe),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            7.0f, 8.0f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_rejects_missing_current_feedback(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Unsafe),
        static_cast<int>(homing_logic::chooseProbeEscapeDirection(
            0.01f, 0.02f, 6.0f, 0.15f, 0.25f)));
}

void test_probe_signal_distinguishes_valid_tie_from_missing_feedback(void) {
    TEST_ASSERT_TRUE(homing_logic::hasProbeSignal(
        0.140f, 0.139f, 0.05f));
    TEST_ASSERT_FALSE(homing_logic::hasProbeSignal(
        0.01f, 0.02f, 0.05f));
}

void test_wiggle_targets_span_fifteen_millimeters_of_travel(void) {
    const homing_logic::WiggleTargets targets =
        homing_logic::calculateWiggleTargets(100, 50);
    TEST_ASSERT_EQUAL_INT32(50, targets.negative);
    TEST_ASSERT_EQUAL_INT32(150, targets.positive);
    TEST_ASSERT_EQUAL_UINT32(150, targets.totalTravel);
}

void test_wiggle_escapes_the_current_limited_direction(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Positive),
        static_cast<int>(homing_logic::chooseWiggleEscapeDirection(
            0.25f, 0.12f, true, false, 0.05f, 0.05f)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Negative),
        static_cast<int>(homing_logic::chooseWiggleEscapeDirection(
            0.12f, 0.25f, false, true, 0.05f, 0.05f)));
}

void test_wiggle_rejects_two_current_limited_directions(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Unsafe),
        static_cast<int>(homing_logic::chooseWiggleEscapeDirection(
            0.25f, 0.25f, true, true, 0.05f, 0.05f)));
}

void test_wiggle_uses_seed_direction_when_both_sides_complete(void) {
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Positive),
        static_cast<int>(homing_logic::chooseWiggleEscapeDirection(
            0.140f, 0.139f, false, false, 0.05f, 0.05f,
            homing_logic::ProbeDirection::Positive)));
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(homing_logic::ProbeDirection::Unsafe),
        static_cast<int>(homing_logic::chooseWiggleEscapeDirection(
            0.01f, 0.02f, false, false, 0.05f, 0.05f,
            homing_logic::ProbeDirection::Positive)));
}

void test_adaptive_current_limit_uses_free_direction(void) {
    TEST_ASSERT_EQUAL_FLOAT(
        4.0f, homing_logic::adaptiveCurrentLimit(7.0f, 1.0f, 6.0f, 1.5f));
    TEST_ASSERT_EQUAL_FLOAT(
        6.0f, homing_logic::adaptiveCurrentLimit(5.0f, 7.0f, 6.0f, 1.5f));
    TEST_ASSERT_FLOAT_WITHIN(
        0.001f, 0.2025f,
        homing_logic::adaptiveCurrentLimit(0.138f, 0.267f, 6.0f, 0.05f));
}

// ─── calculateMeasuredStroke ───

void test_calculateMeasuredStroke_positive_position(void) {
    // position=5000, max=10000 → 5000
    float result = homing_logic::calculateMeasuredStroke(5000, 10000.0f);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, result);
}

void test_calculateMeasuredStroke_negative_position(void) {
    // position=-5000, max=10000 → abs → 5000
    float result = homing_logic::calculateMeasuredStroke(-5000, 10000.0f);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, result);
}

void test_calculateMeasuredStroke_clamped_to_max(void) {
    // position=15000, max=10000 → clamped to 10000
    float result = homing_logic::calculateMeasuredStroke(15000, 10000.0f);
    TEST_ASSERT_EQUAL_FLOAT(10000.0f, result);
}

// ─── calculatePostHomingPosition ───

void test_calculatePostHomingPosition_positive_sign(void) {
    // sign=1, measured=5000, afterHoming=0.5 → goTo=-1*5000=-5000, negative so *0.5 = -2500
    int32_t result = homing_logic::calculatePostHomingPosition(1, 5000.0f, 0.5f);
    TEST_ASSERT_EQUAL_INT32(-2500, result);
}

void test_calculatePostHomingPosition_negative_sign(void) {
    // sign=-1, measured=5000, afterHoming=0.5 → goTo=-(-1)*5000=5000, positive so unchanged = 5000
    int32_t result = homing_logic::calculatePostHomingPosition(-1, 5000.0f, 0.5f);
    TEST_ASSERT_EQUAL_INT32(5000, result);
}

// ─── isHomingTimedOut ───

void test_isHomingTimedOut_not_timed_out(void) {
    // 30000ms, timeout=40000 → false
    TEST_ASSERT_FALSE(homing_logic::isHomingTimedOut(30000, 40000));
}

void test_isHomingTimedOut_timed_out(void) {
    // 41000ms, timeout=40000 → true
    TEST_ASSERT_TRUE(homing_logic::isHomingTimedOut(41000, 40000));
}

// ─── isStrokeTooShortLogic ───

void test_isStrokeTooShort_above_min(void) {
    // measured=100, min=50 → false (100 > 50)
    TEST_ASSERT_FALSE(homing_logic::isStrokeTooShortLogic(100.0f, 50.0f));
}

void test_isStrokeTooShort_below_min(void) {
    // measured=30, min=50 → true (30 <= 50)
    TEST_ASSERT_TRUE(homing_logic::isStrokeTooShortLogic(30.0f, 50.0f));
}

void test_isStrokeTooShort_equal_to_min(void) {
    // measured=50, min=50 → true (50 <= 50, equal)
    TEST_ASSERT_TRUE(homing_logic::isStrokeTooShortLogic(50.0f, 50.0f));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_isCurrentOverLimit_above_threshold);
    RUN_TEST(test_isCurrentOverLimit_below_threshold);
    RUN_TEST(test_isCurrentOverLimit_exactly_at_threshold);
    RUN_TEST(test_probe_escapes_negative_hard_stop);
    RUN_TEST(test_probe_escapes_positive_hard_stop);
    RUN_TEST(test_probe_selects_lower_current_direction);
    RUN_TEST(test_probe_selects_measured_lower_current_direction);
    RUN_TEST(test_probe_rejects_ambiguous_direction);
    RUN_TEST(test_probe_rejects_both_directions_blocked);
    RUN_TEST(test_probe_rejects_missing_current_feedback);
    RUN_TEST(test_probe_signal_distinguishes_valid_tie_from_missing_feedback);
    RUN_TEST(test_wiggle_targets_span_fifteen_millimeters_of_travel);
    RUN_TEST(test_wiggle_escapes_the_current_limited_direction);
    RUN_TEST(test_wiggle_rejects_two_current_limited_directions);
    RUN_TEST(test_wiggle_uses_seed_direction_when_both_sides_complete);
    RUN_TEST(test_adaptive_current_limit_uses_free_direction);

    RUN_TEST(test_calculateMeasuredStroke_positive_position);
    RUN_TEST(test_calculateMeasuredStroke_negative_position);
    RUN_TEST(test_calculateMeasuredStroke_clamped_to_max);

    RUN_TEST(test_calculatePostHomingPosition_positive_sign);
    RUN_TEST(test_calculatePostHomingPosition_negative_sign);

    RUN_TEST(test_isHomingTimedOut_not_timed_out);
    RUN_TEST(test_isHomingTimedOut_timed_out);

    RUN_TEST(test_isStrokeTooShort_above_min);
    RUN_TEST(test_isStrokeTooShort_below_min);
    RUN_TEST(test_isStrokeTooShort_equal_to_min);

    return UNITY_END();
}
