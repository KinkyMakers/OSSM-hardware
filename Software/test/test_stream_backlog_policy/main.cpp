#include <unity.h>

#include "stream_backlog_policy.h"

void test_fresh_bounded_queue_keeps_oldest() {
    TEST_ASSERT_FALSE(stream_backlog_policy::shouldDiscardOldest(
        40, 100, 4));
}

void test_stale_queue_discards_when_newer_request_exists() {
    TEST_ASSERT_TRUE(stream_backlog_policy::shouldDiscardOldest(
        stream_backlog_policy::kMaximumWaypointAgeMilliseconds + 1, 100, 2));
}

void test_excess_duration_discards_even_when_packets_arrived_in_burst() {
    TEST_ASSERT_TRUE(stream_backlog_policy::shouldDiscardOldest(
        0,
        stream_backlog_policy::kMaximumQueuedDurationMilliseconds + 1, 10));
}

void test_newest_request_is_never_discarded() {
    TEST_ASSERT_FALSE(stream_backlog_policy::shouldDiscardOldest(
        5000, 5000, 1));
}

void test_limits_are_inclusive() {
    TEST_ASSERT_FALSE(stream_backlog_policy::shouldDiscardOldest(
        stream_backlog_policy::kMaximumWaypointAgeMilliseconds,
        stream_backlog_policy::kMaximumQueuedDurationMilliseconds, 2));
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_fresh_bounded_queue_keeps_oldest);
    RUN_TEST(test_stale_queue_discards_when_newer_request_exists);
    RUN_TEST(
        test_excess_duration_discards_even_when_packets_arrived_in_burst);
    RUN_TEST(test_newest_request_is_never_discarded);
    RUN_TEST(test_limits_are_inclusive);
    return UNITY_END();
}
