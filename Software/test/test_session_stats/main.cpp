#include "unity.h"
#include "test_helpers.h"

// Mock FastAccelStepper for testing
class MockStepper {
public:
    int32_t currentPosition = 0;
    int32_t targetPosition = 0;

    int32_t getCurrentPosition() { return currentPosition; }
    int32_t targetPos() { return targetPosition; }
};

// Mock OSSM class with just the parts needed for updateStats()
class MockOSSM {
public:
    MockStepper* stepper;
    SessionStatistics sessionStatistics;
    MotionDirection currentMotionDirection = MotionDirection::RETRACTING;
    MotionDirection lastMotionDirection = MotionDirection::RETRACTING;
    int32_t lastPositionForStats = 0;
    bool statsInitialized = false;

    MockOSSM(MockStepper* s) : stepper(s) {}

    void updateStats() {
        // Get current motor state
        int32_t currentPosition = stepper->getCurrentPosition();
        int32_t targetPosition = stepper->targetPos();

        // Determine direction based on where motor is heading
        // Only update direction if motor hasn't reached target (is actively moving toward something)
        MotionDirection newDirection = currentMotionDirection;
        if (currentPosition != targetPosition) {
            newDirection = (currentPosition > targetPosition)
                ? MotionDirection::EXTENDING
                : MotionDirection::RETRACTING;
        }

        // Initialize tracking on first call
        if (!statsInitialized) {
            lastPositionForStats = currentPosition;
            lastMotionDirection = newDirection;
            currentMotionDirection = newDirection;
            statsInitialized = true;
            return;
        }

        // Accumulate distance traveled (convert steps to millimeters)
        // Using 20 steps per mm (1_mm constant from Config)
        double deltaSteps = abs(currentPosition - lastPositionForStats);
        sessionStatistics.distanceInMillimeters += deltaSteps / 20.0;

        // Count strokes on direction changes (happens when target changes)
        if (newDirection != lastMotionDirection) {
            // Count strokes on EXTENDING→RETRACTING transitions (one complete cycle)
            if (lastMotionDirection == MotionDirection::EXTENDING &&
                newDirection == MotionDirection::RETRACTING) {
                sessionStatistics.strokesTotal++;
            }

            // Update direction tracking
            lastMotionDirection = newDirection;
        }

        // Update tracking variables
        currentMotionDirection = newDirection;
        lastPositionForStats = currentPosition;
    }

    void reset() {
        sessionStatistics.strokesTotal = 0;
        sessionStatistics.distanceInMillimeters = 0.0;
        statsInitialized = false;
    }
};

MockStepper* stepper;
MockOSSM* ossm;

void setUp(void) {
    stepper = new MockStepper();
    ossm = new MockOSSM(stepper);
}

void tearDown(void) {
    delete ossm;
    delete stepper;
}

// ============================================================================
// INITIALIZATION TESTS
// ============================================================================

void test_initialization_sets_tracking_state(void) {
    stepper->currentPosition = 0;
    stepper->targetPosition = 0;

    ossm->updateStats();

    TEST_ASSERT_TRUE(ossm->statsInitialized);
    TEST_ASSERT_EQUAL(0, ossm->lastPositionForStats);
    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);
    TEST_ASSERT_EQUAL_FLOAT(0.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_initialization_does_not_count_stroke(void) {
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;

    ossm->updateStats();

    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);
}

// ============================================================================
// DISTANCE ACCUMULATION TESTS
// ============================================================================

void test_distance_accumulates_forward_movement(void) {
    // Initialize at position 0
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // Initialize

    // Move to -1000 (1000 steps = 50mm at 20 steps/mm)
    stepper->currentPosition = -1000;
    ossm->updateStats();

    TEST_ASSERT_EQUAL_FLOAT(50.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_distance_accumulates_backward_movement(void) {
    // Initialize at position -1000
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();  // Initialize

    // Move to 0 (1000 steps = 50mm)
    stepper->currentPosition = 0;
    ossm->updateStats();

    TEST_ASSERT_EQUAL_FLOAT(50.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_distance_accumulates_over_multiple_movements(void) {
    // Initialize at 0
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    // Move to -1000 (50mm)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();
    TEST_ASSERT_EQUAL_FLOAT(50.0, ossm->sessionStatistics.distanceInMillimeters);

    // Move back to 0 (50mm)
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();
    TEST_ASSERT_EQUAL_FLOAT(100.0, ossm->sessionStatistics.distanceInMillimeters);

    // Move to -1000 again (50mm)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();
    TEST_ASSERT_EQUAL_FLOAT(150.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_distance_with_partial_movement(void) {
    // Initialize at 0
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    // Move partway to -500 (500 steps = 25mm)
    stepper->currentPosition = -500;
    ossm->updateStats();

    TEST_ASSERT_EQUAL_FLOAT(25.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_distance_ignores_no_movement(void) {
    // Initialize at 0
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    // Call again without moving
    ossm->updateStats();

    TEST_ASSERT_EQUAL_FLOAT(0.0, ossm->sessionStatistics.distanceInMillimeters);
}

// ============================================================================
// STROKE COUNTING TESTS
// ============================================================================

void test_stroke_count_on_direction_change_extending_to_retracting(void) {
    // Initialize at 0, heading to -1000 (EXTENDING)
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    // Move to -1000, now heading to 0 (RETRACTING)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();

    TEST_ASSERT_EQUAL(1, ossm->sessionStatistics.strokesTotal);
}

void test_stroke_count_on_direction_change_retracting_to_extending(void) {
    // Initialize at -1000, heading to 0 (RETRACTING)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();

    // Move to 0, now heading to -1000 (EXTENDING)
    // This is RETRACTING→EXTENDING, which should NOT count as a stroke
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);
}

void test_stroke_count_multiple_direction_changes(void) {
    // Start at 0, extending
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // Initialize, strokes = 0

    // Move to -1000, retracting (EXTENDING→RETRACTING)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();  // Counts as stroke, strokes = 1

    // Move to 0, extending (RETRACTING→EXTENDING)
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // Does NOT count, strokes = 1

    // Move to -1000, retracting (EXTENDING→RETRACTING)
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();  // Counts as stroke, strokes = 2

    TEST_ASSERT_EQUAL(2, ossm->sessionStatistics.strokesTotal);
}

void test_no_stroke_count_when_direction_unchanged(void) {
    // Initialize at 0, extending
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    // Move partway, still extending
    stepper->currentPosition = -500;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);

    // Move more, still extending
    stepper->currentPosition = -800;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);
}

// ============================================================================
// DIRECTION DETECTION TESTS
// ============================================================================

void test_direction_extending_when_current_greater_than_target(void) {
    stepper->currentPosition = 0;
    stepper->targetPosition = -500;

    ossm->updateStats();

    TEST_ASSERT_EQUAL(MotionDirection::EXTENDING, ossm->currentMotionDirection);
}

void test_direction_retracting_when_current_less_than_target(void) {
    stepper->currentPosition = -500;
    stepper->targetPosition = 0;

    ossm->updateStats();

    TEST_ASSERT_EQUAL(MotionDirection::RETRACTING, ossm->currentMotionDirection);
}

void test_direction_retracting_when_current_equals_target(void) {
    stepper->currentPosition = 0;
    stepper->targetPosition = 0;

    ossm->updateStats();

    // When equal, currentPosition < targetPosition is false, so RETRACTING
    TEST_ASSERT_EQUAL(MotionDirection::RETRACTING, ossm->currentMotionDirection);
}

// ============================================================================
// COMBINED SCENARIO TESTS
// ============================================================================

void test_complete_stroke_cycle_counting_and_distance(void) {
    // Full stroke cycle: 0 → -1000 → 0
    // Expected: 1 stroke (one EXTENDING→RETRACTING transition), 100mm distance

    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // Initialize

    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();  // EXTENDING→RETRACTING: Stroke 1, Distance: 50mm

    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // RETRACTING→EXTENDING: No stroke counted, Distance: 100mm

    TEST_ASSERT_EQUAL(1, ossm->sessionStatistics.strokesTotal);
    TEST_ASSERT_EQUAL_FLOAT(100.0, ossm->sessionStatistics.distanceInMillimeters);
}

void test_incremental_movement_within_stroke(void) {
    // Simulating continuous movement in small increments
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();  // Initialize

    // Move in 100-step increments (5mm each)
    for (int pos = -100; pos >= -1000; pos -= 100) {
        stepper->currentPosition = pos;
        stepper->targetPosition = -1000;
        ossm->updateStats();
    }

    // Should accumulate 50mm (1000 steps) with no stroke counted yet
    TEST_ASSERT_EQUAL_FLOAT(50.0, ossm->sessionStatistics.distanceInMillimeters);
    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);

    // Now reverse direction
    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();  // Direction change, stroke counted

    TEST_ASSERT_EQUAL(1, ossm->sessionStatistics.strokesTotal);
}

void test_reset_clears_statistics(void) {
    // Accumulate some data
    stepper->currentPosition = 0;
    stepper->targetPosition = -1000;
    ossm->updateStats();

    stepper->currentPosition = -1000;
    stepper->targetPosition = 0;
    ossm->updateStats();

    TEST_ASSERT_GREATER_THAN(0, ossm->sessionStatistics.strokesTotal);
    TEST_ASSERT_GREATER_THAN(0.0, ossm->sessionStatistics.distanceInMillimeters);

    // Reset
    ossm->reset();

    TEST_ASSERT_EQUAL(0, ossm->sessionStatistics.strokesTotal);
    TEST_ASSERT_EQUAL_FLOAT(0.0, ossm->sessionStatistics.distanceInMillimeters);
    TEST_ASSERT_FALSE(ossm->statsInitialized);
}

// ============================================================================
// TEST RUNNER
// ============================================================================

int runUnityTests() {
    UNITY_BEGIN();

    // Initialization tests
    RUN_TEST(test_initialization_sets_tracking_state);
    RUN_TEST(test_initialization_does_not_count_stroke);

    // Distance accumulation tests
    RUN_TEST(test_distance_accumulates_forward_movement);
    RUN_TEST(test_distance_accumulates_backward_movement);
    RUN_TEST(test_distance_accumulates_over_multiple_movements);
    RUN_TEST(test_distance_with_partial_movement);
    RUN_TEST(test_distance_ignores_no_movement);

    // Stroke counting tests
    RUN_TEST(test_stroke_count_on_direction_change_extending_to_retracting);
    RUN_TEST(test_stroke_count_on_direction_change_retracting_to_extending);
    RUN_TEST(test_stroke_count_multiple_direction_changes);
    RUN_TEST(test_no_stroke_count_when_direction_unchanged);

    // Direction detection tests
    RUN_TEST(test_direction_extending_when_current_greater_than_target);
    RUN_TEST(test_direction_retracting_when_current_less_than_target);
    RUN_TEST(test_direction_retracting_when_current_equals_target);

    // Combined scenario tests
    RUN_TEST(test_complete_stroke_cycle_counting_and_distance);
    RUN_TEST(test_incremental_movement_within_stroke);
    RUN_TEST(test_reset_clears_statistics);

    return UNITY_END();
}

int main(void) {
    return runUnityTests();
}
