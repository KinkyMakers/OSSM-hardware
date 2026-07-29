#include <unity.h>

#include "communication_priority_policy.h"

using communication_priority_policy::RadioPreference;

void test_balanced_policy_keeps_background_network_available() {
    const auto policy = communication_priority_policy::forStreamingMode(false);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioPreference::Balanced),
                          static_cast<int>(policy.radioPreference));
    TEST_ASSERT_TRUE(policy.allowBackgroundNetworkWork);
}

void test_streaming_policy_prioritizes_ble_and_defers_background_work() {
    const auto policy = communication_priority_policy::forStreamingMode(true);
    TEST_ASSERT_EQUAL_INT(static_cast<int>(RadioPreference::Bluetooth),
                          static_cast<int>(policy.radioPreference));
    TEST_ASSERT_FALSE(policy.allowBackgroundNetworkWork);
}

void test_secondary_service_budgets_stay_bounded() {
    TEST_ASSERT_EQUAL_UINT32(
        250,
        communication_priority_policy::kBackgroundDeferralPollMilliseconds);
    TEST_ASSERT_EQUAL_UINT32(
        60000,
        communication_priority_policy::kPairingStatusPollMilliseconds);
    TEST_ASSERT_EQUAL_INT(2,
                          communication_priority_policy::kMqttTaskPriority);
    TEST_ASSERT_EQUAL_INT(6144,
                          communication_priority_policy::kMqttTaskStackBytes);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_balanced_policy_keeps_background_network_available);
    RUN_TEST(test_streaming_policy_prioritizes_ble_and_defers_background_work);
    RUN_TEST(test_secondary_service_budgets_stay_bounded);
    return UNITY_END();
}
