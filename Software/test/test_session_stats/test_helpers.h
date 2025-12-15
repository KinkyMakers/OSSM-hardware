#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

/**
 * Test helper definitions for session statistics testing.
 * These mirror the types defined in OSSM.h but are independent
 * to allow isolated unit testing.
 */

enum class MotionDirection {
    EXTENDING,   // Moving away from home (negative position, inserting)
    RETRACTING   // Moving toward home (zero position, withdrawing)
};

struct SessionStatistics {
    int strokesTotal = 0;
    double distanceInMillimeters = 0.0;
};

#endif // TEST_HELPERS_H
