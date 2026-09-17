#include <unity.h>
#include "simple_pen_logic.h"

void setUp(void) {}
void tearDown(void) {}

// ─── calculateSpeed ───

void test_calculateSpeed_zero_percent(void) {
    // 0% → 0
    float result = simple_pen_logic::calculateSpeed(0.0f, 500.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_calculateSpeed_100_percent(void) {
    // 100%, maxSpeed=500, stepsPerMm=10 → 10*500*100/100 = 5000
    float result = simple_pen_logic::calculateSpeed(100.0f, 500.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(5000.0f, result);
}

void test_calculateSpeed_50_percent(void) {
    // 50%, maxSpeed=500, stepsPerMm=10 → 2500
    float result = simple_pen_logic::calculateSpeed(50.0f, 500.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(2500.0f, result);
}

// ─── calculateAcceleration ───

void test_calculateAcceleration_zero_percent(void) {
    // 0% → 0
    float result = simple_pen_logic::calculateAcceleration(0.0f, 500.0f, 100.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, result);
}

void test_calculateAcceleration_100_percent(void) {
    // 100%, maxSpeed=500, scaling=100, stepsPerMm=10 → 10*500*100*100/100 = 500000
    float result = simple_pen_logic::calculateAcceleration(100.0f, 500.0f, 100.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(500000.0f, result);
}

void test_calculateAcceleration_50_percent(void) {
    // 50% → quadratic: 10*500*50*50/100 = 125000
    float result = simple_pen_logic::calculateAcceleration(50.0f, 500.0f, 100.0f, 10.0f);
    TEST_ASSERT_EQUAL_FLOAT(125000.0f, result);
}

// ─── isInDeadZone ───

void test_isInDeadZone_below_threshold(void) {
    // knob=1.0, deadZone=2.0 → true
    TEST_ASSERT_TRUE(simple_pen_logic::isInDeadZone(1.0f, 2.0f));
}

void test_isInDeadZone_above_threshold(void) {
    // knob=3.0, deadZone=2.0 → false
    TEST_ASSERT_FALSE(simple_pen_logic::isInDeadZone(3.0f, 2.0f));
}

void test_isInDeadZone_at_threshold(void) {
    // knob=2.0, deadZone=2.0 → false (not strictly less)
    TEST_ASSERT_FALSE(simple_pen_logic::isInDeadZone(2.0f, 2.0f));
}

// ─── isSpeedChangeSignificant ───

void test_isSpeedChangeSignificant_not_significant(void) {
    // old=50, new=60, deadZone=2 → |10| > 5*2=10 → false
    TEST_ASSERT_FALSE(simple_pen_logic::isSpeedChangeSignificant(50.0f, 60.0f, 2.0f));
}

void test_isSpeedChangeSignificant_significant(void) {
    // old=50, new=61, deadZone=2 → |11| > 10 → true
    TEST_ASSERT_TRUE(simple_pen_logic::isSpeedChangeSignificant(50.0f, 61.0f, 2.0f));
}

// ─── calculateTarget ───

void test_calculateTarget_forward(void) {
    // isForward=true, stroke=50%, measured=10000 → -abs(0.5*10000) = -5000
    int32_t result = simple_pen_logic::calculateTarget(true, 50.0f, 10000.0f);
    TEST_ASSERT_EQUAL_INT32(-5000, result);
}

void test_calculateTarget_backward(void) {
    // isForward=false → 0
    int32_t result = simple_pen_logic::calculateTarget(false, 50.0f, 10000.0f);
    TEST_ASSERT_EQUAL_INT32(0, result);
}

// ─── calculateStrokeDistance ───

void test_calculateStrokeDistance_full_stroke(void) {
    // stroke=100%, measured=10000, stepsPerMm=10 → (1.0*10000/10)/1000 = 1.0
    double result = simple_pen_logic::calculateStrokeDistance(100.0f, 10000.0f, 10.0f);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 1.0, result);
}

void test_calculateStrokeDistance_half_stroke(void) {
    // stroke=50% → 0.5
    double result = simple_pen_logic::calculateStrokeDistance(50.0f, 10000.0f, 10.0f);
    TEST_ASSERT_DOUBLE_WITHIN(0.0001, 0.5, result);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_calculateSpeed_zero_percent);
    RUN_TEST(test_calculateSpeed_100_percent);
    RUN_TEST(test_calculateSpeed_50_percent);

    RUN_TEST(test_calculateAcceleration_zero_percent);
    RUN_TEST(test_calculateAcceleration_100_percent);
    RUN_TEST(test_calculateAcceleration_50_percent);

    RUN_TEST(test_isInDeadZone_below_threshold);
    RUN_TEST(test_isInDeadZone_above_threshold);
    RUN_TEST(test_isInDeadZone_at_threshold);

    RUN_TEST(test_isSpeedChangeSignificant_not_significant);
    RUN_TEST(test_isSpeedChangeSignificant_significant);

    RUN_TEST(test_calculateTarget_forward);
    RUN_TEST(test_calculateTarget_backward);

    RUN_TEST(test_calculateStrokeDistance_full_stroke);
    RUN_TEST(test_calculateStrokeDistance_half_stroke);

    return UNITY_END();
}
