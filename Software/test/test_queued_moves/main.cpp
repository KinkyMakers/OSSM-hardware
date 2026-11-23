#include <unity.h>

#include <cstring>
#include <vector>

#include "queued_moves.h"

using namespace queued_moves;

void setUp(void) {}
void tearDown(void) {}

static std::vector<uint8_t> command(std::initializer_list<uint8_t> payload) {
    std::vector<uint8_t> bytes = {'q', ':'};
    bytes.insert(bytes.end(), payload);
    return bytes;
}

// ─── parseQueueCommand ───

void test_parse_empty_is_clear(void) {
    auto bytes = command({});
    auto parsed = parseQueueCommand(bytes.data(), bytes.size());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL(0, parsed->count);
}

void test_parse_pairs(void) {
    auto bytes = command({0, 100, 255, 0});
    auto parsed = parseQueueCommand(bytes.data(), bytes.size());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL(2, parsed->count);
    TEST_ASSERT_EQUAL_UINT8(0, parsed->payload[0]);
    TEST_ASSERT_EQUAL_UINT8(100, parsed->payload[1]);
    TEST_ASSERT_EQUAL_UINT8(255, parsed->payload[2]);
}

void test_parse_odd_length_rejected(void) {
    auto bytes = command({10, 20, 30});
    TEST_ASSERT_FALSE(parseQueueCommand(bytes.data(), bytes.size()).has_value());
}

void test_parse_ten_moves_accepted_eleven_rejected(void) {
    std::vector<uint8_t> bytes = {'q', ':'};
    bytes.resize(2 + 2 * kMaxMovesPerCommand, 1);
    TEST_ASSERT_TRUE(parseQueueCommand(bytes.data(), bytes.size()).has_value());
    bytes.resize(bytes.size() + 2, 1);
    TEST_ASSERT_FALSE(parseQueueCommand(bytes.data(), bytes.size()).has_value());
}

void test_parse_wrong_prefix_rejected(void) {
    const uint8_t bytes[] = {'q', ';', 1, 2};
    TEST_ASSERT_FALSE(isQueueCommand(bytes, sizeof(bytes)));
    TEST_ASSERT_FALSE(parseQueueCommand(bytes, sizeof(bytes)).has_value());
    const uint8_t shortBytes[] = {'q'};
    TEST_ASSERT_FALSE(isQueueCommand(shortBytes, sizeof(shortBytes)));
}

// ─── MoveBuffer ───

void test_buffer_fifo_order(void) {
    MoveBuffer buffer;
    const uint8_t payload[] = {1, 10, 2, 20, 3, 30};
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 3));
    Move move;
    for (uint8_t i = 1; i <= 3; ++i) {
        TEST_ASSERT_TRUE(buffer.pop(move));
        TEST_ASSERT_EQUAL_UINT16(i * 257, move.position);
        TEST_ASSERT_EQUAL_UINT8(i * 10, move.timeMs);
    }
    TEST_ASSERT_FALSE(buffer.pop(move));
}

void test_buffer_full_rejects_whole_command(void) {
    MoveBuffer buffer;
    uint8_t payload[2 * kMaxMovesPerCommand] = {};
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 10));
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 10));
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 10));
    TEST_ASSERT_EQUAL(30, buffer.size());
    TEST_ASSERT_FALSE(buffer.pushAll(payload, 3));  // only 2 free
    TEST_ASSERT_EQUAL(30, buffer.size());
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 2));
    TEST_ASSERT_EQUAL(kBufferSize, buffer.size());
}

void test_buffer_wraps_around(void) {
    MoveBuffer buffer;
    Move move;
    uint8_t next = 0;
    uint8_t expected = 0;
    // Push and pop enough to wrap the 32-slot buffer several times.
    for (int round = 0; round < 20; ++round) {
        uint8_t payload[2 * 7];
        for (int i = 0; i < 7; ++i) {
            payload[2 * i] = next;
            payload[2 * i + 1] = static_cast<uint8_t>(next + 1);
            ++next;
        }
        TEST_ASSERT_TRUE(buffer.pushAll(payload, 7));
        for (int i = 0; i < 7; ++i) {
            TEST_ASSERT_TRUE(buffer.pop(move));
            TEST_ASSERT_EQUAL_UINT16(expected * 257, move.position);
            TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(expected + 1),
                                    move.timeMs);
            ++expected;
        }
    }
    TEST_ASSERT_TRUE(buffer.empty());
}

void test_buffer_clear(void) {
    MoveBuffer buffer;
    const uint8_t payload[] = {1, 1, 2, 2};
    buffer.pushAll(payload, 2);
    buffer.clear();
    Move move;
    TEST_ASSERT_TRUE(buffer.empty());
    TEST_ASSERT_FALSE(buffer.pop(move));
    TEST_ASSERT_EQUAL(kBufferSize, buffer.freeSlots());
}

void test_buffer_total_ms(void) {
    MoveBuffer buffer;
    const uint8_t payload[] = {1, 100, 2, 200, 3, 255};
    buffer.pushAll(payload, 3);
    TEST_ASSERT_EQUAL_UINT32(555, buffer.totalMs());
    Move move;
    buffer.pop(move);
    TEST_ASSERT_EQUAL_UINT32(455, buffer.totalMs());
    // A rejected push changes nothing.
    uint8_t big[2 * kMaxMovesPerCommand] = {};
    for (int i = 0; i < 3; ++i) {
        buffer.pushAll(big, 10);
    }
    TEST_ASSERT_FALSE(buffer.pushAll(payload, 3));
    TEST_ASSERT_EQUAL_UINT32(455, buffer.totalMs());
    buffer.clear();
    TEST_ASSERT_EQUAL_UINT32(0, buffer.totalMs());
}

// ─── lowWaterSlots ───

void test_low_water_slots(void) {
    TEST_ASSERT_EQUAL(500, kDefaultLowWaterMs);
    // Enough buffered: 0.
    TEST_ASSERT_EQUAL_UINT8(0, lowWaterSlots(500, 500, 20));
    TEST_ASSERT_EQUAL_UINT8(0, lowWaterSlots(900, 500, 20));
    // Below the mark: free slots.
    TEST_ASSERT_EQUAL_UINT8(20, lowWaterSlots(499, 500, 20));
    TEST_ASSERT_EQUAL_UINT8(32, lowWaterSlots(0, 500, 32));
    // Low water 0: never low.
    TEST_ASSERT_EQUAL_UINT8(0, lowWaterSlots(0, 0, 32));
}

// ─── scaleQueuedPosition ───

void test_scale_ends_of_window(void) {
    // maxStroke=4000, depth=1000
    TEST_ASSERT_EQUAL_FLOAT(-1000, scaleQueuedPosition(65535, 4000, 1000));
    TEST_ASSERT_EQUAL_FLOAT(-5000, scaleQueuedPosition(0, 4000, 1000));
    TEST_ASSERT_FLOAT_WITHIN(0.1, -3000, scaleQueuedPosition(32768, 4000, 1000));
}

// ─── decodePosition ───

void test_decode_full_range(void) {
    TEST_ASSERT_EQUAL_UINT16(0, decodePosition(0, Range{}));
    TEST_ASSERT_EQUAL_UINT16(65535, decodePosition(255, Range{}));
    TEST_ASSERT_EQUAL_UINT16(128 * 257, decodePosition(128, Range{}));
}

void test_decode_narrow_range(void) {
    const Range range{1000, 1510};  // span 510: 2 per byte
    TEST_ASSERT_EQUAL_UINT16(1000, decodePosition(0, range));
    TEST_ASSERT_EQUAL_UINT16(1002, decodePosition(1, range));
    TEST_ASSERT_EQUAL_UINT16(1510, decodePosition(255, range));
    // Rounds to nearest: span 100, byte 1 -> 0.39 -> 0, byte 2 -> 0.78 -> 1.
    TEST_ASSERT_EQUAL_UINT16(5000, decodePosition(1, Range{5000, 5100}));
    TEST_ASSERT_EQUAL_UINT16(5001, decodePosition(2, Range{5000, 5100}));
}

void test_decode_empty_range(void) {
    const Range range{4242, 4242};
    TEST_ASSERT_EQUAL_UINT16(4242, decodePosition(0, range));
    TEST_ASSERT_EQUAL_UINT16(4242, decodePosition(255, range));
}

void test_buffer_decodes_with_range(void) {
    MoveBuffer buffer;
    const uint8_t payload[] = {0, 25, 255, 25};
    TEST_ASSERT_TRUE(buffer.pushAll(payload, 2, Range{30000, 30255}));
    Move move;
    buffer.pop(move);
    TEST_ASSERT_EQUAL_UINT16(30000, move.position);
    buffer.pop(move);
    TEST_ASSERT_EQUAL_UINT16(30255, move.position);
}

// ─── parseRangeCommand ───

static std::vector<uint8_t> rangeCommand(uint16_t min, uint16_t max,
                                         std::initializer_list<uint8_t> moves) {
    std::vector<uint8_t> bytes = {'r', ':',
                                  static_cast<uint8_t>(min & 0xFF),
                                  static_cast<uint8_t>(min >> 8),
                                  static_cast<uint8_t>(max & 0xFF),
                                  static_cast<uint8_t>(max >> 8)};
    bytes.insert(bytes.end(), moves);
    return bytes;
}

void test_parse_range_only(void) {
    auto bytes = rangeCommand(0x1234, 0xABCD, {});
    auto parsed = parseRangeCommand(bytes.data(), bytes.size());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL_UINT16(0x1234, parsed->range.min);
    TEST_ASSERT_EQUAL_UINT16(0xABCD, parsed->range.max);
    TEST_ASSERT_EQUAL(0, parsed->count);
}

void test_parse_range_with_moves(void) {
    auto bytes = rangeCommand(100, 200, {0, 25, 255, 30});
    auto parsed = parseRangeCommand(bytes.data(), bytes.size());
    TEST_ASSERT_TRUE(parsed.has_value());
    TEST_ASSERT_EQUAL(2, parsed->count);
    TEST_ASSERT_EQUAL_UINT8(255, parsed->payload[2]);
    TEST_ASSERT_EQUAL_UINT8(30, parsed->payload[3]);
}

void test_parse_range_rejects(void) {
    // Too short for the range.
    const uint8_t shortBytes[] = {'r', ':', 1, 2, 3};
    TEST_ASSERT_FALSE(parseRangeCommand(shortBytes, sizeof(shortBytes)).has_value());
    // Odd move bytes.
    auto odd = rangeCommand(0, 10, {1, 2, 3});
    TEST_ASSERT_FALSE(parseRangeCommand(odd.data(), odd.size()).has_value());
    // min > max.
    auto backwards = rangeCommand(11, 10, {});
    TEST_ASSERT_FALSE(
        parseRangeCommand(backwards.data(), backwards.size()).has_value());
    // Wrong prefix.
    const uint8_t q[] = {'q', ':', 0, 0, 0, 0};
    TEST_ASSERT_FALSE(isRangeCommand(q, sizeof(q)));
    TEST_ASSERT_FALSE(parseRangeCommand(q, sizeof(q)).has_value());
}

void test_parse_range_eight_moves_fit_in_one_packet(void) {
    std::vector<uint8_t> bytes = rangeCommand(0, 100, {});
    bytes.resize(6 + 2 * kMaxMovesPerRangeCommand, 1);
    TEST_ASSERT_TRUE(bytes.size() <= 22);
    TEST_ASSERT_TRUE(parseRangeCommand(bytes.data(), bytes.size()).has_value());
    bytes.resize(bytes.size() + 2, 1);
    TEST_ASSERT_FALSE(parseRangeCommand(bytes.data(), bytes.size()).has_value());
}

int main(int argc, char **argv) {
    UNITY_BEGIN();

    RUN_TEST(test_parse_empty_is_clear);
    RUN_TEST(test_parse_pairs);
    RUN_TEST(test_parse_odd_length_rejected);
    RUN_TEST(test_parse_ten_moves_accepted_eleven_rejected);
    RUN_TEST(test_parse_wrong_prefix_rejected);

    RUN_TEST(test_buffer_fifo_order);
    RUN_TEST(test_buffer_full_rejects_whole_command);
    RUN_TEST(test_buffer_wraps_around);
    RUN_TEST(test_buffer_clear);
    RUN_TEST(test_buffer_total_ms);
    RUN_TEST(test_low_water_slots);

    RUN_TEST(test_scale_ends_of_window);

    RUN_TEST(test_decode_full_range);
    RUN_TEST(test_decode_narrow_range);
    RUN_TEST(test_decode_empty_range);
    RUN_TEST(test_buffer_decodes_with_range);

    RUN_TEST(test_parse_range_only);
    RUN_TEST(test_parse_range_with_moves);
    RUN_TEST(test_parse_range_rejects);
    RUN_TEST(test_parse_range_eight_moves_fit_in_one_packet);

    return UNITY_END();
}
