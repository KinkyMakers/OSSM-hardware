#include <unity.h>

#include <cmath>
#include <cstring>
#include <iterator>

#include "status_packet.h"

using namespace status_packet;

void setUp(void) {}
void tearDown(void) {}

using Value = StateName::Value;

// The state numbering is a wire contract with clients: pin it.
void test_state_ids_are_stable(void) {
    TEST_ASSERT_EQUAL_UINT8(0, StateName("idle").id());
    TEST_ASSERT_EQUAL_UINT8(5, StateName("menu.idle").id());
    TEST_ASSERT_EQUAL_UINT8(12, StateName("strokeEngine.pattern").id());
    TEST_ASSERT_EQUAL_UINT8(13, StateName("streaming").id());
    TEST_ASSERT_EQUAL_UINT8(14, StateName("streaming.idle").id());
    TEST_ASSERT_EQUAL_UINT8(15, StateName("streaming.preflight").id());
    TEST_ASSERT_EQUAL_UINT8(36, StateName("pairing.failed").id());
    TEST_ASSERT_EQUAL(37, std::size(StateName::kNames));
}

void test_unknown_state(void) {
    TEST_ASSERT_TRUE(StateName("nonsense") == StateName(Value::Unknown));
    TEST_ASSERT_TRUE(StateName("").value() == Value::Unknown);
    TEST_ASSERT_TRUE(StateName().value() == Value::Unknown);
    TEST_ASSERT_EQUAL_UINT8(255, StateName().id());
    TEST_ASSERT_EQUAL_STRING("unknown", StateName().to_string());
}

void test_state_names_round_trip(void) {
    // Every name maps to its own id, in order, and back to the same name.
    for (size_t i = 0; i < std::size(StateName::kNames); ++i) {
        const StateName::Entry& entry = StateName::kNames[i];
        TEST_ASSERT_EQUAL_UINT8(i, static_cast<uint8_t>(entry.value));
        const StateName state(std::string_view(entry.name));
        TEST_ASSERT_TRUE(state.value() == entry.value);
        TEST_ASSERT_EQUAL_STRING(entry.name, state.to_string());
    }
    TEST_ASSERT_EQUAL_STRING("streaming.idle",
                             StateName(Value::StreamingIdle).to_string());
}

void test_encode_little_endian(void) {
    uint8_t out[kPacketSize];
    encode({Value::StreamingIdle, 50, 80, 100, 0x1234, 7}, 0xABCD, out);
    const uint8_t expected[kPacketSize] = {14, 50, 80, 100,
                                           0x34, 0x12, 0xCD, 0xAB, 7};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, out, kPacketSize);
}

void test_low_water_rate_limit(void) {
    TEST_ASSERT_FALSE(lowWaterSendAllowed(kLowWaterMinIntervalMs - 1));
    TEST_ASSERT_TRUE(lowWaterSendAllowed(kLowWaterMinIntervalMs));
    TEST_ASSERT_EQUAL(100, kLowWaterMinIntervalMs);
}

void test_clamp_percent(void) {
    TEST_ASSERT_EQUAL_UINT8(0, clampPercent(-5.0f));
    TEST_ASSERT_EQUAL_UINT8(0, clampPercent(NAN));
    TEST_ASSERT_EQUAL_UINT8(42, clampPercent(41.6f));
    TEST_ASSERT_EQUAL_UINT8(100, clampPercent(150.0f));
}

void test_position_tenths(void) {
    TEST_ASSERT_EQUAL_UINT16(0, positionTenths(-3.0f));
    TEST_ASSERT_EQUAL_UINT16(0, positionTenths(NAN));
    TEST_ASSERT_EQUAL_UINT16(1235, positionTenths(123.46f));
    TEST_ASSERT_EQUAL_UINT16(65535, positionTenths(7000.0f));
}

void test_timestamp_delta(void) {
    TEST_ASSERT_EQUAL_UINT16(20, timestampDelta(1020, 1000));
    TEST_ASSERT_EQUAL_UINT16(65535, timestampDelta(100000, 0));
    // millis() wrap-around
    TEST_ASSERT_EQUAL_UINT16(15, timestampDelta(5, 0xFFFFFFF6u));
}

void test_position_alone_does_not_trigger_send(void) {
    const Status a = {Value::StreamingIdle, 50, 80, 100, 1000};
    Status b = a;
    b.positionTenthsMm = 1500;
    TEST_ASSERT_FALSE(settingsOrStateChanged(a, b));
    b = a;
    b.state = Value::Streaming;
    TEST_ASSERT_TRUE(settingsOrStateChanged(a, b));
    b = a;
    b.speed = 51;
    TEST_ASSERT_TRUE(settingsOrStateChanged(a, b));
    b = a;
    b.stroke = 79;
    TEST_ASSERT_TRUE(settingsOrStateChanged(a, b));
    b = a;
    b.depth = 99;
    TEST_ASSERT_TRUE(settingsOrStateChanged(a, b));
}

void test_should_send(void) {
    TEST_ASSERT_TRUE(shouldSend(true, 0));
    TEST_ASSERT_FALSE(shouldSend(false, kHeartbeatMs - 1));
    TEST_ASSERT_TRUE(shouldSend(false, kHeartbeatMs));
}

void test_packet_fits_one_att_payload(void) {
    TEST_ASSERT_LESS_OR_EQUAL(20, kPacketSize);
}

int main(int argc, char **argv) {
    UNITY_BEGIN();
    RUN_TEST(test_state_ids_are_stable);
    RUN_TEST(test_unknown_state);
    RUN_TEST(test_state_names_round_trip);
    RUN_TEST(test_encode_little_endian);
    RUN_TEST(test_low_water_rate_limit);
    RUN_TEST(test_clamp_percent);
    RUN_TEST(test_position_tenths);
    RUN_TEST(test_timestamp_delta);
    RUN_TEST(test_position_alone_does_not_trigger_send);
    RUN_TEST(test_should_send);
    RUN_TEST(test_packet_fits_one_att_payload);
    return UNITY_END();
}
