#include <unity.h>

#include <algorithm>
#include <cmath>

#include "queued_moves.h"
#include "trajectory.h"

using namespace trajectory;

void setUp(void) {}
void tearDown(void) {}

// A stroke sampled every 25 ms, the way XBounce sends it: 0 -> 100 -> 0.
static Trajectory stroke() {
    Trajectory t;
    const float points[] = {0, 25, 50, 75, 100, 75, 50, 25, 0};
    for (size_t i = 0; i < 9; ++i) {
        t.push(1000 + 25 * i, points[i]);
    }
    return t;
}

// ─── Trajectory ───

void test_passes_through_keyframes(void) {
    Trajectory t = stroke();
    for (size_t i = 0; i < t.size(); ++i) {
        TEST_ASSERT_FLOAT_WITHIN(1e-3, t[i].position,
            t.positionAt(t[i].timeMs));
    }
}

void test_holds_before_and_after(void) {
    Trajectory t = stroke();
    TEST_ASSERT_EQUAL_FLOAT(0, t.positionAt(500));
    TEST_ASSERT_EQUAL_FLOAT(0, t.positionAt(5000));
}

void test_empty_is_zero(void) {
    Trajectory t;
    TEST_ASSERT_EQUAL_FLOAT(0, t.positionAt(123));
}

void test_does_not_stop_between_points(void) {
    // On a straight run the speed mid-way through the run should stay near
    // the run's speed (1 step/ms), not dip to 0 at each point.
    Trajectory t = stroke();
    for (uint32_t ms = 1025; ms < 1075; ++ms) {
        const float v = t.positionAt(ms + 1) - t.positionAt(ms);
        TEST_ASSERT_FLOAT_WITHIN(0.3, 1.0, v);
    }
}

void test_no_overshoot_at_peak(void) {
    Trajectory t = stroke();
    for (uint32_t ms = 1000; ms <= 1200; ++ms) {
        const float p = t.positionAt(ms);
        TEST_ASSERT_TRUE(p >= -1e-3f);
        TEST_ASSERT_TRUE(p <= 100.0f + 1e-3f);
    }
}

void test_monotone_within_segments(void) {
    // Uneven steps and spacing: each segment stays between its two points.
    Trajectory t;
    t.push(0, 0);
    t.push(10, 90);
    t.push(60, 100);
    t.push(70, 20);
    t.push(200, 0);
    for (size_t i = 0; i + 1 < t.size(); ++i) {
        const float lo = std::fmin(t[i].position, t[i + 1].position);
        const float hi = std::fmax(t[i].position, t[i + 1].position);
        for (uint32_t ms = t[i].timeMs; ms <= t[i + 1].timeMs; ++ms) {
            const float p = t.positionAt(ms);
            TEST_ASSERT_TRUE(p >= lo - 1e-3f);
            TEST_ASSERT_TRUE(p <= hi + 1e-3f);
        }
    }
}

void test_flat_at_turning_point_and_end(void) {
    Trajectory t = stroke();
    // Near the peak (1100) and the end (1200) the path barely moves.
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0, t.positionAt(1101) - t.positionAt(1100));
    TEST_ASSERT_FLOAT_WITHIN(0.1, 0, t.positionAt(1200) - t.positionAt(1199));
}

void test_continuous_across_keyframes(void) {
    Trajectory t = stroke();
    for (size_t i = 1; i + 1 < t.size(); ++i) {
        const uint32_t k = t[i].timeMs;
        const float before = t.positionAt(k) - t.positionAt(k - 1);
        const float after = t.positionAt(k + 1) - t.positionAt(k);
        TEST_ASSERT_FLOAT_WITHIN(0.2, before, after);
    }
}

void test_push_same_time_replaces(void) {
    Trajectory t;
    t.push(10, 1);
    t.push(10, 5);
    TEST_ASSERT_EQUAL(1, t.size());
    TEST_ASSERT_EQUAL_FLOAT(5, t[0].position);
}

void test_push_earlier_ignored(void) {
    Trajectory t;
    t.push(10, 1);
    TEST_ASSERT_FALSE(t.push(5, 2));
    TEST_ASSERT_EQUAL(1, t.size());
}

void test_push_full(void) {
    Trajectory t;
    for (uint32_t i = 0; i < kMaxKeyframes; ++i) {
        TEST_ASSERT_TRUE(t.push(i, 0));
    }
    TEST_ASSERT_FALSE(t.push(100, 0));
}

void test_keep_last(void) {
    Trajectory t = stroke();
    t.keepLast(3);
    TEST_ASSERT_EQUAL(3, t.size());
    TEST_ASSERT_EQUAL_UINT32(1150, t[0].timeMs);
    TEST_ASSERT_EQUAL_UINT32(1200, t.back().timeMs);
}

void test_millis_wrap(void) {
    Trajectory t;
    t.push(0xFFFFFFF0u, 0);
    t.push(0x00000010u, 32);
    TEST_ASSERT_FLOAT_WITHIN(1e-3, 16, t.positionAt(0));
    TEST_ASSERT_EQUAL_FLOAT(32, t.positionAt(0x20));
}

// ─── lookahead / speed ───

void test_lookahead_clamped(void) {
    TEST_ASSERT_EQUAL_UINT32(kMinLookaheadMs, lookaheadMs(1000, 1000000));
    TEST_ASSERT_EQUAL_UINT32(kMaxLookaheadMs, lookaheadMs(20000, 1000));
    TEST_ASSERT_EQUAL_UINT32(kMaxLookaheadMs, lookaheadMs(100, 0));
    TEST_ASSERT_EQUAL_UINT32(kMaxLookaheadMs, lookaheadMs(0, 1000000));
    // Braking: 1.5 * 20000 / (2 * 200000) s = 75 ms
    TEST_ASSERT_EQUAL_UINT32(75, lookaheadMs(-20000, 200000));
}

void test_lookahead_grows_as_speed_drops(void) {
    // Resolution: 8 steps at 200 steps/s = 40 ms, at 100 steps/s = 80 ms.
    TEST_ASSERT_EQUAL_UINT32(40, lookaheadMs(200, 1000000));
    TEST_ASSERT_EQUAL_UINT32(80, lookaheadMs(-100, 1000000));
    // Every speed aims at least kMinAimSteps ahead, up to the longest
    // lookahead.
    for (float v = 10; v < 20000; v *= 1.1f) {
        const uint32_t h = lookaheadMs(v, 1000000);
        if (h < kMaxLookaheadMs) {
            TEST_ASSERT_TRUE(v * h / 1000.0f >= kMinAimSteps - 0.1f);
        }
    }
}

void test_pursuit_speed(void) {
    TEST_ASSERT_EQUAL_UINT32(1000, pursuitSpeed(-50, 50, 100, 20000));
    TEST_ASSERT_EQUAL_UINT32(100, pursuitSpeed(0, 50, 100, 20000));
    TEST_ASSERT_EQUAL_UINT32(20000, pursuitSpeed(5000, 20, 100, 20000));
}

// ─── MoveBuffer::peek ───

void test_peek(void) {
    queued_moves::MoveBuffer buffer;
    const uint8_t payload[] = {1, 10, 2, 20, 3, 30};
    buffer.pushAll(payload, 3);
    queued_moves::Move move;
    buffer.pop(move);
    TEST_ASSERT_TRUE(buffer.peek(1, move));
    TEST_ASSERT_EQUAL_UINT16(3 * 257, move.position);
    TEST_ASSERT_FALSE(buffer.peek(2, move));
    TEST_ASSERT_EQUAL(2, buffer.size());
}

// ─── per-batch range encoding ───

// A steady move at `mmps` over a 150 mm window (20 steps/mm), sampled every
// 25 ms and sent the way XBounce does: batches of 8 moves, each batch's
// positions encoded as bytes within that batch's own 16-bit range. Returns
// how many of the middle milliseconds the curve is near-stopped.
static int stalledMsWithBatchRanges(float mmps) {
    const float windowSteps = 3000, stepsPerMm = 20;
    Trajectory t;
    const size_t batch = queued_moves::kMaxMovesPerRangeCommand;
    uint16_t positions[batch];
    for (size_t k = 0; k < batch; ++k) {
        const float steps = 500 + mmps * stepsPerMm * 0.025f * k;
        positions[k] = static_cast<uint16_t>(
            std::lround(steps / windowSteps * 65535.0f));
    }
    queued_moves::Range range{positions[0], positions[0]};
    for (uint16_t p : positions) {
        range.min = std::min(range.min, p);
        range.max = std::max(range.max, p);
    }
    const float span = range.max - range.min;
    for (size_t k = 0; k < batch; ++k) {
        const uint8_t byte = span == 0 ? 0 : static_cast<uint8_t>(std::lround(
            (positions[k] - range.min) * 255.0f / span));
        const uint16_t decoded = queued_moves::decodePosition(byte, range);
        t.push(25 * k, decoded / 65535.0f * windowSteps);
    }
    int stalled = 0;
    for (uint32_t ms = 50; ms < 125; ++ms) {
        const float v = (t.positionAt(ms + 1) - t.positionAt(ms)) * 1000;
        if (v < 0.5f * mmps * stepsPerMm) {
            ++stalled;
        }
    }
    return stalled;
}

void test_slow_moves_do_not_stall_with_batch_ranges(void) {
    // With one 8-bit range across the whole stroke, 10 mm/s was near-stopped
    // 75% of the time.
    for (float mmps : {1.f, 2.f, 5.f, 10.f, 20.f}) {
        TEST_ASSERT_EQUAL_INT(0, stalledMsWithBatchRanges(mmps));
    }
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_passes_through_keyframes);
    RUN_TEST(test_holds_before_and_after);
    RUN_TEST(test_empty_is_zero);
    RUN_TEST(test_does_not_stop_between_points);
    RUN_TEST(test_no_overshoot_at_peak);
    RUN_TEST(test_monotone_within_segments);
    RUN_TEST(test_flat_at_turning_point_and_end);
    RUN_TEST(test_continuous_across_keyframes);
    RUN_TEST(test_push_same_time_replaces);
    RUN_TEST(test_push_earlier_ignored);
    RUN_TEST(test_push_full);
    RUN_TEST(test_keep_last);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_lookahead_clamped);
    RUN_TEST(test_lookahead_grows_as_speed_drops);
    RUN_TEST(test_pursuit_speed);
    RUN_TEST(test_peek);
    RUN_TEST(test_slow_moves_do_not_stall_with_batch_ranges);
    return UNITY_END();
}
