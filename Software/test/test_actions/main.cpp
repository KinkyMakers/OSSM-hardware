#include "MockOSSM.h"
#include "state/actions.h"
#include "unity.h"

void test_initDevice_calls_setup_and_homing(void) {
    OSSM ossm;
    initDevice(ossm);

    TEST_ASSERT_EQUAL(1, ossm.setupCalled);
    TEST_ASSERT_EQUAL(1, ossm.findHomeCalled);
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_initDevice_calls_setup_and_homing);
    return UNITY_END();
}

// WARNING!!! PLEASE REMOVE UNNECESSARY MAIN IMPLEMENTATIONS //

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void) { return runUnityTests(); }
