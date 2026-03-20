// ┌──────────────────────────────────────────────────────────────────────────┐
// │ STROKE ENGINE PATTERNS — UNIT TESTS                                    │
// │                                                                        │
// │ Tests for lib/StrokeEngine/src/pattern.h:                              │
// │   SimpleStroke       — Trapezoidal 1/3-1/3-1/3 profile                 │
// │   TeasingPounding    — In/out speed ratio via sensation                 │
// │   RoboStroke         — Acceleration profile via sensation               │
// │   HalfnHalf          — Alternating half/full strokes                    │
// │   Deeper             — Ramping insertion depth                          │
// │   StopNGo            — Pauses between stroke series                    │
// │   Insist             — Fractional stroke with position bias            │
// │                                                                        │
// │ All 7 concrete patterns derived from Pattern base class.               │
// └──────────────────────────────────────────────────────────────────────────┘

#include <ArduinoFake.h>
#include <unity.h>

using namespace fakeit;

#include "pattern.h"

// ─── Helpers ──────────────────────────────────────────────────────────────

void setupPattern(Pattern& p, int stroke, int depth, float timeOfStroke, float sensation) {
    p.setStroke(stroke);
    p.setDepth(depth);
    p.setTimeOfStroke(timeOfStroke);
    p.setSensation(sensation);
}

// Arduino map() is mocked by ArduinoFake; we supply a real implementation
static long realMap(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void setUp(void) {
    ArduinoFakeReset();
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);

    // map() — redirect to real implementation
    When(Method(ArduinoFake(), map)).AlwaysDo(
        [](long x, long in_min, long in_max, long out_min, long out_max) -> long {
            return realMap(x, in_min, in_max, out_min, out_max);
        });

    // Serial.println overloads used by DEBUG_PATTERN
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const String&))).AlwaysReturn(0);
    When(OverloadedMethod(ArduinoFake(Serial), println, size_t(const char[]))).AlwaysReturn(0);
}

void tearDown(void) {}

// ═══════════════════════════════════════════════════════════════════════════
// SimpleStroke
// ═══════════════════════════════════════════════════════════════════════════

void test_simple_even_index_moves_to_depth() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(5000, m.stroke);
}

void test_simple_odd_index_moves_out() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(5000 - 1000, m.stroke);
}

void test_simple_speed_formula() {
    SimpleStroke p("Simple Stroke");
    // timeOfStroke = 2.0 → internally halved to 1.0
    // speed = 1.5 * stroke / halfTime = 1.5 * 1000 / 1.0 = 1500
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(1500, m.speed);
}

void test_simple_acceleration_formula() {
    SimpleStroke p("Simple Stroke");
    // halfTime = 1.0, speed = 1500
    // accel = 3.0 * 1500 / 1.0 = 4500
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(4500, m.acceleration);
}

void test_simple_sensation_has_no_effect() {
    SimpleStroke p1("Simple Stroke");
    SimpleStroke p2("Simple Stroke");
    setupPattern(p1, 1000, 5000, 2.0, 0);
    setupPattern(p2, 1000, 5000, 2.0, 75);
    motionParameter m1 = p1.nextTarget(0);
    motionParameter m2 = p2.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(m1.speed, m2.speed);
    TEST_ASSERT_EQUAL_INT(m1.acceleration, m2.acceleration);
    TEST_ASSERT_EQUAL_INT(m1.stroke, m2.stroke);
}

void test_simple_alternates_direction() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(0).stroke);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(2).stroke);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(3).stroke);
}

void test_simple_zero_stroke_gives_zero_speed() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 0, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(0, m.speed);
    TEST_ASSERT_EQUAL_INT(0, m.acceleration);
}

void test_simple_skip_is_false() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_FALSE(m.skip);
}

void test_simple_name() {
    SimpleStroke p("Simple Stroke");
    TEST_ASSERT_EQUAL_STRING("Simple Stroke", p.getName());
}

void test_simple_various_stroke_values() {
    SimpleStroke p("Simple Stroke");
    setupPattern(p, 2000, 8000, 4.0, 0);
    // halfTime = 2.0, speed = 1.5 * 2000 / 2.0 = 1500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(1500, m.speed);
    TEST_ASSERT_EQUAL_INT(8000, m.stroke);
}

// ═══════════════════════════════════════════════════════════════════════════
// TeasingPounding
// ═══════════════════════════════════════════════════════════════════════════

void test_teasing_zero_sensation_equal_speed() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    // With sensation=0, fscale returns 1.0, so fastStroke = 0.5*2.0/1.0 = 1.0
    // in and out times are both 1.0
    TEST_ASSERT_EQUAL_INT(mIn.speed, mOut.speed);
}

void test_teasing_positive_sensation_in_faster() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, 50);
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    // Positive sensation → in-stroke faster → higher speed on even index
    TEST_ASSERT_TRUE(mIn.speed > mOut.speed);
}

void test_teasing_negative_sensation_out_faster() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, -50);
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    // Negative sensation → out-stroke faster → higher speed on odd index
    TEST_ASSERT_TRUE(mOut.speed > mIn.speed);
}

void test_teasing_even_stroke_is_depth() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(5000, m.stroke);
}

void test_teasing_odd_stroke_is_depth_minus_stroke() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(4000, m.stroke);
}

void test_teasing_max_positive_sensation_speed_ratio() {
    TeasingPounding p("Teasing or Pounding");
    // sensation=100 → fscale(0,100,1,5,100,0) = 5.0
    // fastStroke = 0.5 * 2.0 / 5.0 = 0.2
    // inStroke = 0.2, outStroke = 2.0 - 0.2 = 1.8
    // inSpeed = 1.5 * 1000 / 0.2 = 7500
    // outSpeed = 1.5 * 1000 / 1.8 = 833
    setupPattern(p, 1000, 5000, 2.0, 100);
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_INT_WITHIN(1, 7500, mIn.speed);
    TEST_ASSERT_EQUAL_INT(int(1.5 * 1000 / 1.8), mOut.speed);
}

void test_teasing_skip_is_false() {
    TeasingPounding p("Teasing or Pounding");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_FALSE(p.nextTarget(0).skip);
}

void test_teasing_name() {
    TeasingPounding p("Teasing or Pounding");
    TEST_ASSERT_EQUAL_STRING("Teasing or Pounding", p.getName());
}

void test_teasing_speed_increases_with_sensation() {
    TeasingPounding p1("TP");
    TeasingPounding p2("TP");
    setupPattern(p1, 1000, 5000, 2.0, 20);
    setupPattern(p2, 1000, 5000, 2.0, 80);
    // Higher sensation → faster in-stroke
    motionParameter m1 = p1.nextTarget(0);
    motionParameter m2 = p2.nextTarget(0);
    TEST_ASSERT_TRUE(m2.speed > m1.speed);
}

void test_teasing_alternates_position() {
    TeasingPounding p("TP");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(0).stroke);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(2).stroke);
}

// ═══════════════════════════════════════════════════════════════════════════
// RoboStroke
// ═══════════════════════════════════════════════════════════════════════════

void test_robo_zero_sensation_standard_trapezoidal() {
    RoboStroke p("Robo Stroke");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // _x = 1/3, halfTime = 1.0
    // speed = 1000 / ((1 - 1/3) * 1.0) = 1000 / 0.6667 = 1500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(1500, m.speed);
}

void test_robo_zero_sensation_matches_simple_stroke() {
    RoboStroke r("Robo");
    SimpleStroke s("Simple");
    setupPattern(r, 1000, 5000, 2.0, 0);
    setupPattern(s, 1000, 5000, 2.0, 0);
    motionParameter mr = r.nextTarget(0);
    motionParameter ms = s.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(ms.speed, mr.speed);
}

void test_robo_positive_sensation_x_approaches_half() {
    RoboStroke p("Robo");
    // sensation=100 → _x = fscale(0,100,1/3,0.5,100,0) = 0.5
    // speed = 1000 / ((1 - 0.5) * 1.0) = 2000
    setupPattern(p, 1000, 5000, 2.0, 100);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(2000, m.speed);
}

void test_robo_negative_sensation_x_approaches_005() {
    RoboStroke p("Robo");
    // sensation=-100 → _x = fscale(0,100,1/3,0.05,100,0) = 0.05
    // speed = 1000 / ((1 - 0.05) * 1.0) = 1000 / 0.95 = 1052
    setupPattern(p, 1000, 5000, 2.0, -100);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(int(1000.0f / 0.95f), m.speed);
}

void test_robo_speed_formula_with_x() {
    RoboStroke p("Robo");
    // sensation=0 → _x = 1/3
    // speed = stroke / ((1 - _x) * halfTime) = 1000 / (2/3 * 1.0) = 1500
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    float expectedX = 1.0f / 3.0f;
    int expectedSpeed = int(1000.0f / ((1.0f - expectedX) * 1.0f));
    TEST_ASSERT_EQUAL_INT(expectedSpeed, m.speed);
}

void test_robo_acceleration_formula() {
    RoboStroke p("Robo");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m = p.nextTarget(0);
    float x = 1.0f / 3.0f;
    float halfTime = 1.0f;
    float speed = 1000.0f / ((1.0f - x) * halfTime);
    int expectedAccel = int(speed / (x * halfTime));
    TEST_ASSERT_EQUAL_INT(expectedAccel, m.acceleration);
}

void test_robo_even_moves_to_depth() {
    RoboStroke p("Robo");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(0).stroke);
}

void test_robo_odd_moves_out() {
    RoboStroke p("Robo");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
}

void test_robo_positive_sens_higher_speed_than_zero() {
    RoboStroke p1("R1");
    RoboStroke p2("R2");
    setupPattern(p1, 1000, 5000, 2.0, 0);
    setupPattern(p2, 1000, 5000, 2.0, 50);
    TEST_ASSERT_TRUE(p2.nextTarget(0).speed > p1.nextTarget(0).speed);
}

void test_robo_name() {
    RoboStroke p("Robo Stroke");
    TEST_ASSERT_EQUAL_STRING("Robo Stroke", p.getName());
}

// ═══════════════════════════════════════════════════════════════════════════
// HalfnHalf
// ═══════════════════════════════════════════════════════════════════════════

void test_halfnhalf_first_stroke_is_half() {
    HalfnHalf p("Half'n'Half");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // index=0 → _half = true, stroke = 500
    // even in-stroke: position = (depth - stroke) + stroke/2 = 4000 + 500 = 4500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(4500, m.stroke);
}

void test_halfnhalf_half_alternates_on_odd() {
    HalfnHalf p("Half'n'Half");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // index 0: _half=true, even → stroke=500, pos = 4500
    p.nextTarget(0);
    // index 1: odd → _half toggles to false, out-stroke: pos = 4000
    p.nextTarget(1);
    // index 2: _half=false, even → stroke=1000, pos = (4000) + 1000 = 5000
    motionParameter m2 = p.nextTarget(2);
    TEST_ASSERT_EQUAL_INT(5000, m2.stroke);
}

void test_halfnhalf_full_cycle_positions() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // index 0: _half=true, half in → 4500
    TEST_ASSERT_EQUAL_INT(4500, p.nextTarget(0).stroke);
    // index 1: odd, _half toggles → false, out → 4000
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
    // index 2: _half=false, full in → 5000
    TEST_ASSERT_EQUAL_INT(5000, p.nextTarget(2).stroke);
    // index 3: odd, _half toggles → true, out → 4000
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(3).stroke);
    // index 4: _half=true, half in → 4500
    TEST_ASSERT_EQUAL_INT(4500, p.nextTarget(4).stroke);
}

void test_halfnhalf_half_speed_is_lower() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // index 0: half stroke → speed uses stroke/2 = 500
    motionParameter mHalf = p.nextTarget(0);
    // After 0→1→2, _half=false, full stroke
    p.nextTarget(1);
    motionParameter mFull = p.nextTarget(2);
    TEST_ASSERT_TRUE(mFull.speed > mHalf.speed);
}

void test_halfnhalf_positive_sensation_in_faster() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 50);
    // Need to get past initial half stroke
    p.nextTarget(0);
    p.nextTarget(1);
    // index 2: _half=false (full stroke), even, in-stroke
    motionParameter mIn = p.nextTarget(2);
    // index 3: odd, out-stroke
    motionParameter mOut = p.nextTarget(3);
    TEST_ASSERT_TRUE(mIn.speed > mOut.speed);
}

void test_halfnhalf_negative_sensation_out_faster() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, -50);
    p.nextTarget(0);
    p.nextTarget(1);
    motionParameter mIn = p.nextTarget(2);
    motionParameter mOut = p.nextTarget(3);
    TEST_ASSERT_TRUE(mOut.speed > mIn.speed);
}

void test_halfnhalf_zero_sensation_equal_speed() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 0);
    p.nextTarget(0);
    p.nextTarget(1);
    motionParameter mIn = p.nextTarget(2);
    motionParameter mOut = p.nextTarget(3);
    // With full stroke, both should use same speed
    TEST_ASSERT_EQUAL_INT(mIn.speed, mOut.speed);
}

void test_halfnhalf_out_stroke_always_depth_minus_stroke() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 0);
    p.nextTarget(0);
    // All odd strokes go to depth - stroke
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
    p.nextTarget(2);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(3).stroke);
}

void test_halfnhalf_name() {
    HalfnHalf p("Half'n'Half");
    TEST_ASSERT_EQUAL_STRING("Half'n'Half", p.getName());
}

void test_halfnhalf_skip_is_false() {
    HalfnHalf p("HH");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_FALSE(p.nextTarget(0).skip);
}

// ═══════════════════════════════════════════════════════════════════════════
// Deeper
// ═══════════════════════════════════════════════════════════════════════════

void test_deeper_amplitude_ramps_up() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // sensation=0 → countStrokesForRamp = map(0, 0, 100, 11, 32) = 11
    // slope = 1000 / 11 = 90 (int division)
    // cycleIndex for index 0: (0/2) % 11 + 1 = 1 → amplitude = 90
    // cycleIndex for index 2: (2/2) % 11 + 1 = 2 → amplitude = 180
    motionParameter m0 = p.nextTarget(0);
    motionParameter m2 = p.nextTarget(2);
    TEST_ASSERT_TRUE(m2.stroke > m0.stroke);
}

void test_deeper_cycle_index_calculation() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // countStrokesForRamp = 11, slope = 90
    // index 0: cycleIndex = 1, amplitude = 90
    // in-stroke: (5000 - 1000) + 90 = 4090
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(4090, m.stroke);
}

void test_deeper_odd_stroke_always_at_base() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // Odd strokes always return depth - stroke = 4000
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(1).stroke);
    TEST_ASSERT_EQUAL_INT(4000, p.nextTarget(3).stroke);
}

void test_deeper_speed_scales_with_amplitude() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, 0);
    motionParameter m0 = p.nextTarget(0);
    motionParameter m2 = p.nextTarget(2);
    // Higher cycleIndex → larger amplitude → higher speed
    TEST_ASSERT_TRUE(m2.speed > m0.speed);
}

void test_deeper_negative_sensation_fewer_ramp_strokes() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // countStrokesForRamp = map(-100, -100, 0, 2, 11) = 2
    // slope = 1000 / 2 = 500
    // index 0: cycleIndex=1, amplitude=500 → pos = 4000+500 = 4500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(4500, m.stroke);
}

void test_deeper_positive_sensation_more_ramp_strokes() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, 100);
    // countStrokesForRamp = map(100, 0, 100, 11, 32) = 32
    // slope = 1000 / 32 = 31
    // index 0: cycleIndex=1, amplitude=31 → pos = 4000+31 = 4031
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(4031, m.stroke);
}

void test_deeper_cycle_wraps_around() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // countStrokesForRamp = 2, slope = 500
    // index 0: cycleIndex = (0/2)%2+1 = 1, amp=500
    // index 2: cycleIndex = (2/2)%2+1 = 2, amp=1000
    // index 4: cycleIndex = (4/2)%2+1 = 1, amp=500 (wraps!)
    motionParameter m0 = p.nextTarget(0);
    motionParameter m4 = p.nextTarget(4);
    TEST_ASSERT_EQUAL_INT(m0.stroke, m4.stroke);
}

void test_deeper_speed_formula() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // halfTime = 1.0, countStrokes = 2, slope = 500
    // index 0: amplitude = 500
    // speed = 1.5 * 500 / 1.0 = 750
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(750, m.speed);
}

void test_deeper_acceleration_formula() {
    Deeper p("Deeper");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // speed = 750, accel = 3.0 * 750 / 1.0 = 2250
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(2250, m.acceleration);
}

void test_deeper_name() {
    Deeper p("Deeper");
    TEST_ASSERT_EQUAL_STRING("Deeper", p.getName());
}

// ═══════════════════════════════════════════════════════════════════════════
// StopNGo
// ═══════════════════════════════════════════════════════════════════════════

void test_stopngo_normal_stroke_when_not_delayed() {
    StopNGo p("Stop'n'Go");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // sensation=-100 → _delayInMillis=100, _startDelayMillis=0
    // Need millis > (0 + 100) to not be delayed
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_FALSE(m.skip);
    TEST_ASSERT_EQUAL_INT(5000, m.stroke);
}

void test_stopngo_even_moves_to_depth() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(5000, m.stroke);
}

void test_stopngo_odd_moves_out() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    // First do an even to increment _strokeIndex
    p.nextTarget(0);
    motionParameter m = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(4000, m.stroke);
}

void test_stopngo_delay_causes_skip() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // sensation=-100 → delay = map(-100, -100, 100, 100, 10000) = 100ms

    // Do a full stroke series of 1 to trigger delay
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    p.nextTarget(0); // even: _strokeIndex becomes 1
    p.nextTarget(1); // odd: _strokeIndex(1) >= _strokeSeriesIndex(1) → _startDelay()

    // Now at time 50ms, should still be delayed (delay = 100ms)
    When(Method(ArduinoFake(), millis)).AlwaysReturn(50);
    motionParameter m = p.nextTarget(2);
    TEST_ASSERT_TRUE(m.skip);
}

void test_stopngo_delay_expires_resumes() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // delay = 100ms

    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    p.nextTarget(0);
    p.nextTarget(1); // triggers delay at millis=0

    // After delay expires (millis > startDelay + delay = 0 + 100)
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    motionParameter m = p.nextTarget(2);
    TEST_ASSERT_FALSE(m.skip);
}

void test_stopngo_speed_formula() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // halfTime = 1.0, speed = 1.5 * 1000 / 1.0 = 1500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(1500, m.speed);
}

void test_stopngo_negative_sensation_short_delay() {
    StopNGo p("SNG");
    // sensation=-100 → delay = map(-100, -100, 100, 100, 10000) = 100ms
    setupPattern(p, 1000, 5000, 2.0, -100);

    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    p.nextTarget(0);
    p.nextTarget(1);

    // At 101ms the delay (100ms from start=0) should have expired
    When(Method(ArduinoFake(), millis)).AlwaysReturn(101);
    motionParameter m = p.nextTarget(2);
    TEST_ASSERT_FALSE(m.skip);
}

void test_stopngo_positive_sensation_long_delay() {
    StopNGo p("SNG");
    // sensation=100 → delay = map(100, -100, 100, 100, 10000) = 10000ms
    setupPattern(p, 1000, 5000, 2.0, 100);

    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    p.nextTarget(0);
    p.nextTarget(1);

    // At 5000ms, still within 10000ms delay
    When(Method(ArduinoFake(), millis)).AlwaysReturn(5000);
    motionParameter m = p.nextTarget(2);
    TEST_ASSERT_TRUE(m.skip);
}

void test_stopngo_stroke_series_increments() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // delay = 100ms

    // First series: 1 stroke
    When(Method(ArduinoFake(), millis)).AlwaysReturn(0);
    p.nextTarget(0); // even, _strokeIndex=1
    p.nextTarget(1); // odd, triggers delay, _strokeSeriesIndex becomes 2

    // After delay, second series should allow 2 strokes before delay
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    p.nextTarget(2); // even, _strokeIndex=1
    motionParameter m3 = p.nextTarget(3); // odd, _strokeIndex(1) < _strokeSeriesIndex(2) → no delay
    TEST_ASSERT_FALSE(m3.skip);

    p.nextTarget(4); // even, _strokeIndex=2
    p.nextTarget(5); // odd, _strokeIndex(2) >= _strokeSeriesIndex(2) → delay starts

    When(Method(ArduinoFake(), millis)).AlwaysReturn(201);
    motionParameter m6 = p.nextTarget(6);
    TEST_ASSERT_TRUE(m6.skip);
}

void test_stopngo_name() {
    StopNGo p("Stop'n'Go");
    TEST_ASSERT_EQUAL_STRING("Stop'n'Go", p.getName());
}

void test_stopngo_skip_is_false_initially() {
    StopNGo p("SNG");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // Must exceed _startDelayMillis(0) + _delayInMillis(100)
    When(Method(ArduinoFake(), millis)).AlwaysReturn(200);
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_FALSE(m.skip);
}

// ═══════════════════════════════════════════════════════════════════════════
// Insist
// ═══════════════════════════════════════════════════════════════════════════

void test_insist_zero_sensation_like_simple() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 0);
    // strokeFraction = (100 - 0) / 100 = 1.0
    // realStroke = 1000 * 1.0 = 1000
    // sensation=0 → strokeInFront = false
    // even: (depth - stroke) + realStroke = 4000 + 1000 = 5000
    // odd: depth - stroke = 4000
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(5000, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(4000, mOut.stroke);
}

void test_insist_stroke_fraction_calculation() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 60);
    // strokeFraction = (100 - 60) / 100 = 0.4
    // realStroke = 1000 * 0.4 = 400
    // strokeInFront = true (positive)
    // even: depth = 5000
    // odd: depth - realStroke = 5000 - 400 = 4600
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(5000, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(4600, mOut.stroke);
}

void test_insist_positive_sensation_front_bias() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 50);
    // strokeFraction = 0.5, realStroke = 500
    // strokeInFront = true
    // even: depth = 5000
    // odd: depth - realStroke = 4500
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(5000, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(4500, mOut.stroke);
}

void test_insist_negative_sensation_back_bias() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, -50);
    // strokeFraction = 0.5, realStroke = 500
    // strokeInFront = false
    // even: (depth - stroke) + realStroke = 4000 + 500 = 4500
    // odd: depth - stroke = 4000
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(4500, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(4000, mOut.stroke);
}

void test_insist_speed_constant_based_on_full_stroke() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 50);
    // speed = 1.5 * stroke / halfTime = 1.5 * 1000 / 1.0 = 1500
    motionParameter m = p.nextTarget(0);
    TEST_ASSERT_EQUAL_INT(1500, m.speed);
}

void test_insist_speed_same_regardless_of_sensation() {
    Insist p1("I1");
    Insist p2("I2");
    setupPattern(p1, 1000, 5000, 2.0, 0);
    setupPattern(p2, 1000, 5000, 2.0, 80);
    TEST_ASSERT_EQUAL_INT(p1.nextTarget(0).speed, p2.nextTarget(0).speed);
}

void test_insist_max_sensation_tiny_stroke() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 100);
    // strokeFraction = (100 - 100) / 100 = 0.0
    // realStroke = 0
    // strokeInFront = true
    // even: depth = 5000, odd: depth - 0 = 5000
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(5000, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(5000, mOut.stroke);
}

void test_insist_negative_max_sensation_tiny_stroke() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, -100);
    // strokeFraction = 0.0, realStroke = 0
    // strokeInFront = false
    // even: (depth - stroke) + 0 = 4000
    // odd: depth - stroke = 4000
    motionParameter mIn = p.nextTarget(0);
    motionParameter mOut = p.nextTarget(1);
    TEST_ASSERT_EQUAL_INT(4000, mIn.stroke);
    TEST_ASSERT_EQUAL_INT(4000, mOut.stroke);
}

void test_insist_skip_is_false() {
    Insist p("Insist");
    setupPattern(p, 1000, 5000, 2.0, 0);
    TEST_ASSERT_FALSE(p.nextTarget(0).skip);
}

void test_insist_name() {
    Insist p("Insist");
    TEST_ASSERT_EQUAL_STRING("Insist", p.getName());
}

// ─── Runner ──────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    // SimpleStroke (~10 tests)
    RUN_TEST(test_simple_even_index_moves_to_depth);
    RUN_TEST(test_simple_odd_index_moves_out);
    RUN_TEST(test_simple_speed_formula);
    RUN_TEST(test_simple_acceleration_formula);
    RUN_TEST(test_simple_sensation_has_no_effect);
    RUN_TEST(test_simple_alternates_direction);
    RUN_TEST(test_simple_zero_stroke_gives_zero_speed);
    RUN_TEST(test_simple_skip_is_false);
    RUN_TEST(test_simple_name);
    RUN_TEST(test_simple_various_stroke_values);

    // TeasingPounding (~10 tests)
    RUN_TEST(test_teasing_zero_sensation_equal_speed);
    RUN_TEST(test_teasing_positive_sensation_in_faster);
    RUN_TEST(test_teasing_negative_sensation_out_faster);
    RUN_TEST(test_teasing_even_stroke_is_depth);
    RUN_TEST(test_teasing_odd_stroke_is_depth_minus_stroke);
    RUN_TEST(test_teasing_max_positive_sensation_speed_ratio);
    RUN_TEST(test_teasing_skip_is_false);
    RUN_TEST(test_teasing_name);
    RUN_TEST(test_teasing_speed_increases_with_sensation);
    RUN_TEST(test_teasing_alternates_position);

    // RoboStroke (~10 tests)
    RUN_TEST(test_robo_zero_sensation_standard_trapezoidal);
    RUN_TEST(test_robo_zero_sensation_matches_simple_stroke);
    RUN_TEST(test_robo_positive_sensation_x_approaches_half);
    RUN_TEST(test_robo_negative_sensation_x_approaches_005);
    RUN_TEST(test_robo_speed_formula_with_x);
    RUN_TEST(test_robo_acceleration_formula);
    RUN_TEST(test_robo_even_moves_to_depth);
    RUN_TEST(test_robo_odd_moves_out);
    RUN_TEST(test_robo_positive_sens_higher_speed_than_zero);
    RUN_TEST(test_robo_name);

    // HalfnHalf (~10 tests)
    RUN_TEST(test_halfnhalf_first_stroke_is_half);
    RUN_TEST(test_halfnhalf_half_alternates_on_odd);
    RUN_TEST(test_halfnhalf_full_cycle_positions);
    RUN_TEST(test_halfnhalf_half_speed_is_lower);
    RUN_TEST(test_halfnhalf_positive_sensation_in_faster);
    RUN_TEST(test_halfnhalf_negative_sensation_out_faster);
    RUN_TEST(test_halfnhalf_zero_sensation_equal_speed);
    RUN_TEST(test_halfnhalf_out_stroke_always_depth_minus_stroke);
    RUN_TEST(test_halfnhalf_name);
    RUN_TEST(test_halfnhalf_skip_is_false);

    // Deeper (~10 tests)
    RUN_TEST(test_deeper_amplitude_ramps_up);
    RUN_TEST(test_deeper_cycle_index_calculation);
    RUN_TEST(test_deeper_odd_stroke_always_at_base);
    RUN_TEST(test_deeper_speed_scales_with_amplitude);
    RUN_TEST(test_deeper_negative_sensation_fewer_ramp_strokes);
    RUN_TEST(test_deeper_positive_sensation_more_ramp_strokes);
    RUN_TEST(test_deeper_cycle_wraps_around);
    RUN_TEST(test_deeper_speed_formula);
    RUN_TEST(test_deeper_acceleration_formula);
    RUN_TEST(test_deeper_name);

    // StopNGo (~10 tests)
    RUN_TEST(test_stopngo_normal_stroke_when_not_delayed);
    RUN_TEST(test_stopngo_even_moves_to_depth);
    RUN_TEST(test_stopngo_odd_moves_out);
    RUN_TEST(test_stopngo_delay_causes_skip);
    RUN_TEST(test_stopngo_delay_expires_resumes);
    RUN_TEST(test_stopngo_speed_formula);
    RUN_TEST(test_stopngo_negative_sensation_short_delay);
    RUN_TEST(test_stopngo_positive_sensation_long_delay);
    RUN_TEST(test_stopngo_stroke_series_increments);
    RUN_TEST(test_stopngo_name);
    RUN_TEST(test_stopngo_skip_is_false_initially);

    // Insist (~10 tests)
    RUN_TEST(test_insist_zero_sensation_like_simple);
    RUN_TEST(test_insist_stroke_fraction_calculation);
    RUN_TEST(test_insist_positive_sensation_front_bias);
    RUN_TEST(test_insist_negative_sensation_back_bias);
    RUN_TEST(test_insist_speed_constant_based_on_full_stroke);
    RUN_TEST(test_insist_speed_same_regardless_of_sensation);
    RUN_TEST(test_insist_max_sensation_tiny_stroke);
    RUN_TEST(test_insist_negative_max_sensation_tiny_stroke);
    RUN_TEST(test_insist_skip_is_false);
    RUN_TEST(test_insist_name);

    return UNITY_END();
}
