#include "unity.h"
#include "utils/format.h"

void test_ZeroSeconds(void) {
    TEST_ASSERT_EQUAL_STRING("0s", formatTime(0).c_str());
}

void test_SingleMinute(void) {
    TEST_ASSERT_EQUAL_STRING("01:00", formatTime(60000).c_str());
}

void test_MultipleUnits(void) {
    TEST_ASSERT_EQUAL_STRING(
        "2d 3h",
        formatTime(183600000).c_str());  // 2 days and 3 hours
}

void test_EdgeOfUnits(void) {
    TEST_ASSERT_EQUAL_STRING("59s", formatTime(59000).c_str());
    TEST_ASSERT_EQUAL_STRING(
        "59:59",
        formatTime(3599000).c_str());  // 59 minutes, 59 seconds
}

void test_CombinedUnits(void) {
    TEST_ASSERT_EQUAL_STRING(
        "1d 1h 1:01",
        formatTime(90061000).c_str());  // 1 day, 1 hour, 1 minute, 1 second
}

void test_Zero(void) {
    TEST_ASSERT_EQUAL_STRING("0 in", formatImperial(0).c_str());
}

void test_LessThanOneFoot(void) {
    TEST_ASSERT_EQUAL_STRING("3 in",
                             formatImperial(0.0762).c_str());  // 3 inches
}

void test_ExactlyOneFoot(void) {
    TEST_ASSERT_EQUAL_STRING("1.00 ft",
                             formatImperial(0.3048).c_str());  // 1 foot
}

void test_MultipleFeet(void) {
    TEST_ASSERT_EQUAL_STRING("100.00 ft",
                             formatImperial(30.48).c_str());  // 100 feet
}

void test_NearlyOneMile(void) {
    TEST_ASSERT_EQUAL_STRING(
        "5279.00 ft", formatImperial(1609.0).c_str());  // Just below one mile
}

void test_ExactlyOneMile(void) {
    TEST_ASSERT_EQUAL_STRING(
        "1.00 mi", formatImperial(1609.344).c_str());  // Exactly one mile
}

void test_MultipleMiles(void) {
    TEST_ASSERT_EQUAL_STRING(
        "3.11 mi", formatImperial(5000).c_str());  // More than 3 miles
}

void test_VerySmallValues(void) {
    TEST_ASSERT_EQUAL_STRING(
        "0 in",
        formatImperial(0.0001).c_str());  // Should round down to 0 inches
}

void test_NegativeValues(void) {
    TEST_ASSERT_EQUAL_STRING(
        "-394 in",
        formatImperial(-10)
            .c_str());  // Assuming your function handles negatives
}

void test_ZeroMeters(void) {
    TEST_ASSERT_EQUAL_STRING("0.0 cm", formatMetric(0).c_str());
}

void test_LessThanOneMeter(void) {
    TEST_ASSERT_EQUAL_STRING("99.0 cm", formatMetric(0.99).c_str());
}

void test_ExactlyOneMeter(void) {
    TEST_ASSERT_EQUAL_STRING("1.0 m", formatMetric(1.0).c_str());
}

void test_MultipleMetersUnder100(void) {
    TEST_ASSERT_EQUAL_STRING("50.0 m", formatMetric(50).c_str());
}

void test_JustBelow100Meters(void) {
    TEST_ASSERT_EQUAL_STRING("99.9 m", formatMetric(99.9).c_str());
}

void test_Exactly100Meters(void) {
    TEST_ASSERT_EQUAL_STRING("100.00 m", formatMetric(100).c_str());
}

void test_Between100And1000Meters(void) {
    TEST_ASSERT_EQUAL_STRING("500.00 m", formatMetric(500).c_str());
}

void test_JustBelow1000Meters(void) {
    TEST_ASSERT_EQUAL_STRING("999.00 m", formatMetric(999).c_str());
}

void test_Exactly1000Meters(void) {
    TEST_ASSERT_EQUAL_STRING("1.00 km", formatMetric(1000).c_str());
}

void test_Above1000Meters(void) {
    TEST_ASSERT_EQUAL_STRING("2.50 km", formatMetric(2500).c_str());
}

void test_VerySmallValuesMetric(void) {
    TEST_ASSERT_EQUAL_STRING("0.1 cm", formatMetric(0.001).c_str());
}

void test_NegativeValuesMetric(void) {
    // Assuming you want to handle negative values explicitly
    TEST_ASSERT_EQUAL_STRING(
        "-1.0 m",
        formatMetric(-1.0).c_str());  // Depends on your function's behavior
}

int runUnityTests() {
    UNITY_BEGIN();
    // Time formatting tests
    RUN_TEST(test_ZeroSeconds);
    RUN_TEST(test_SingleMinute);
    RUN_TEST(test_MultipleUnits);
    RUN_TEST(test_EdgeOfUnits);
    RUN_TEST(test_CombinedUnits);

    // Imperial distance formatting tests
    RUN_TEST(test_Zero);
    RUN_TEST(test_LessThanOneFoot);
    RUN_TEST(test_ExactlyOneFoot);
    RUN_TEST(test_MultipleFeet);
    RUN_TEST(test_NearlyOneMile);
    RUN_TEST(test_ExactlyOneMile);
    RUN_TEST(test_MultipleMiles);
    RUN_TEST(test_VerySmallValues);
    RUN_TEST(test_NegativeValues);

    // Metric
    RUN_TEST(test_ZeroMeters);
    RUN_TEST(test_LessThanOneMeter);
    RUN_TEST(test_ExactlyOneMeter);
    RUN_TEST(test_MultipleMetersUnder100);
    RUN_TEST(test_JustBelow100Meters);
    RUN_TEST(test_Exactly100Meters);
    RUN_TEST(test_Between100And1000Meters);
    RUN_TEST(test_JustBelow1000Meters);
    RUN_TEST(test_Exactly1000Meters);
    RUN_TEST(test_Above1000Meters);
    RUN_TEST(test_VerySmallValuesMetric);
    RUN_TEST(test_NegativeValuesMetric);
    return UNITY_END();
}

// WARNING!!! PLEASE REMOVE UNNECESSARY MAIN IMPLEMENTATIONS //

/**
 * For native dev-platform or for some embedded frameworks
 */
int main(void) { return runUnityTests(); }
