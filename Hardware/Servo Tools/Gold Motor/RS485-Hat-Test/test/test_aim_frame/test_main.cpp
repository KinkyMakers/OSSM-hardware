/**
 * AimFrame unit tests. Vectors come from the YZ-AIM manual's worked examples
 * and from frames captured on the bench (V2.3 hat + 57AIM30).
 *
 * Run on the board: pio test -e rs485_hat_v23 -f test_aim_frame
 */
#include <Arduino.h>
#include <unity.h>

#include "AimFrame.h"

using namespace AimFrame;

namespace {

    template <size_t N>
    void assertFrame(const uint8_t (&expected)[N], const uint8_t *actual, size_t len) {
        TEST_ASSERT_EQUAL_UINT(N, len);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, actual, N);
    }

}

// ---------------------------------------------------------------- builders

void test_read_manual_example() {
    const uint8_t expected[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x01, 0x84, 0x0A};
    uint8_t out[8];
    assertFrame(expected, out, buildRead(out, 1, 0x0000, 1));
}

void test_read_position_manual_example() {
    const uint8_t expected[] = {0x01, 0x03, 0x00, 0x16, 0x00, 0x02, 0x25, 0xCF};
    uint8_t out[8];
    assertFrame(expected, out, buildRead(out, 1, 0x0016, 2));
}

void test_read_steps_bench_capture() {
    const uint8_t expected[] = {0x01, 0x03, 0x00, 0x0B, 0x00, 0x01, 0xF5, 0xC8};
    uint8_t out[8];
    assertFrame(expected, out, buildRead(out, 1, 0x000B, 1));
}

void test_write_single_manual_examples() {
    uint8_t out[8];
    const uint8_t enable[] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0x48, 0x0A};
    assertFrame(enable, out, buildWriteSingle(out, 1, 0x0000, 1));
    const uint8_t accel[] = {0x01, 0x06, 0x00, 0x03, 0x13, 0x88, 0x74, 0x9C};
    assertFrame(accel, out, buildWriteSingle(out, 1, 0x0003, 5000));
    const uint8_t speed[] = {0x01, 0x06, 0x00, 0x02, 0x05, 0xDC, 0x2A, 0xC3};
    assertFrame(speed, out, buildWriteSingle(out, 1, 0x0002, 1500));
    const uint8_t save[] = {0x01, 0x06, 0x00, 0x14, 0x00, 0x01, 0x08, 0x0E};
    assertFrame(save, out, buildWriteSingle(out, 1, 0x0014, 1));
}

void test_write_multiple_incremental_moves() {
    uint8_t out[16];
    // +4000 pulses: low word first
    const uint16_t plus[] = {0x0FA0, 0x0000};
    const uint8_t plusFrame[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0x0F, 0xA0, 0x00, 0x00, 0xF0, 0xCC};
    assertFrame(plusFrame, out, buildWriteMultiple(out, 1, 0x000C, plus, 2));
    // -4000 pulses = 0xFFFFF060
    const uint16_t minus[] = {0xF060, 0xFFFF};
    const uint8_t minusFrame[] = {0x01, 0x10, 0x00, 0x0C, 0x00, 0x02, 0x04, 0xF0, 0x60, 0xFF, 0xFF, 0xC1, 0x54};
    assertFrame(minusFrame, out, buildWriteMultiple(out, 1, 0x000C, minus, 2));
}

void test_write_multiple_absolute_position() {
    uint8_t out[16];
    const uint16_t pos[] = {0x1F40, 0x0000};  // 8000
    const uint8_t frame[] = {0x01, 0x10, 0x00, 0x16, 0x00, 0x02, 0x04, 0x1F, 0x40, 0x00, 0x00, 0x74, 0x89};
    assertFrame(frame, out, buildWriteMultiple(out, 1, 0x0016, pos, 2));
}

void test_move78_manual_example() {
    // 10000 pulses, MSB first
    const uint8_t expected[] = {0x01, 0x78, 0x00, 0x00, 0x27, 0x10, 0xBB, 0xFC};
    uint8_t out[8];
    assertFrame(expected, out, buildMove(out, 1, 10000));
}

void test_move78_negative() {
    uint8_t out[8];
    buildMove(out, 1, -1);
    TEST_ASSERT_EQUAL_HEX8(0xFF, out[2]);
    TEST_ASSERT_EQUAL_HEX8(0xFF, out[5]);
}

// ---------------------------------------------------------------- parser

void test_parse_reply_with_leading_glitch_byte() {
    const uint8_t rx[] = {0x00, 0x01, 0x03, 0x02, 0x03, 0x20, 0xB9, 0x6C};  // bench capture
    Reply r = parseReply(rx, sizeof(rx), 1, fcReadHolding, replyLength(fcReadHolding, 1));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT(1, r.offset);
    TEST_ASSERT_EQUAL_UINT(1, r.skipped);
    TEST_ASSERT_EQUAL_UINT16(800, be16(rx + r.offset + 3));
}

void test_parse_reply_without_glitch_byte() {
    const uint8_t rx[] = {0x01, 0x03, 0x02, 0x00, 0x01, 0x79, 0x84};  // manual example
    Reply r = parseReply(rx, sizeof(rx), 1, fcReadHolding, replyLength(fcReadHolding, 1));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_UINT(0, r.skipped);
}

void test_parse_incomplete_until_full_frame() {
    const uint8_t rx[] = {0x00, 0x01, 0x03, 0x02, 0x03, 0x20, 0xB9, 0x6C};
    const size_t expected = replyLength(fcReadHolding, 1);
    for (size_t n = 0; n < sizeof(rx); n++) {
        Reply r = parseReply(rx, n, 1, fcReadHolding, expected);
        TEST_ASSERT_EQUAL_MESSAGE(static_cast<int>(Status::Incomplete), static_cast<int>(r.status),
                                  "partial frame must not be accepted");
    }
}

void test_parse_bad_crc() {
    const uint8_t rx[] = {0x01, 0x03, 0x02, 0x03, 0x20, 0xB9, 0x6D};
    Reply r = parseReply(rx, sizeof(rx), 1, fcReadHolding, replyLength(fcReadHolding, 1));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::BadCrc), static_cast<int>(r.status));
}

void test_parse_exception() {
    // glitch byte + "illegal data address" exception with a valid CRC
    const uint8_t body[] = {0x01, 0x83, 0x02};
    uint16_t crc = crc16(body, 3);
    uint8_t buf[6] = {0x00, 0x01, 0x83, 0x02, static_cast<uint8_t>(crc & 0xFF), static_cast<uint8_t>(crc >> 8)};
    Reply r = parseReply(buf, sizeof(buf), 1, fcReadHolding, replyLength(fcReadHolding, 1));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Exception), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_HEX8(0x02, r.exceptionCode);
}

void test_parse_wrong_server_and_fc() {
    const uint8_t otherId[] = {0x02, 0x03, 0x02, 0x03, 0x20, 0x00, 0x00};
    TEST_ASSERT_EQUAL(static_cast<int>(Status::BadReply),
                      static_cast<int>(parseReply(otherId, sizeof(otherId), 1, fcReadHolding, 7).status));
    const uint8_t otherFc[] = {0x01, 0x06, 0x00, 0x00, 0x00, 0x01, 0x48, 0x0A};
    TEST_ASSERT_EQUAL(static_cast<int>(Status::BadReply),
                      static_cast<int>(parseReply(otherFc, sizeof(otherFc), 1, fcReadHolding, 7).status));
}

void test_parse_move78_reply_value() {
    // Bench capture body "01 78 FF F9 FF FF" = -7 (low word first)
    uint8_t buf[8] = {0x01, 0x78, 0xFF, 0xF9, 0xFF, 0xFF};
    uint16_t crc = crc16(buf, 6);
    buf[6] = crc & 0xFF;
    buf[7] = crc >> 8;
    Reply r = parseReply(buf, sizeof(buf), 1, fcMove, replyLength(fcMove));
    TEST_ASSERT_EQUAL(static_cast<int>(Status::Ok), static_cast<int>(r.status));
    TEST_ASSERT_EQUAL_INT32(-7, moveReplyValue(buf + r.offset));
}

void setup() {
    delay(2000);  // let the test runner open the port
    UNITY_BEGIN();
    RUN_TEST(test_read_manual_example);
    RUN_TEST(test_read_position_manual_example);
    RUN_TEST(test_read_steps_bench_capture);
    RUN_TEST(test_write_single_manual_examples);
    RUN_TEST(test_write_multiple_incremental_moves);
    RUN_TEST(test_write_multiple_absolute_position);
    RUN_TEST(test_move78_manual_example);
    RUN_TEST(test_move78_negative);
    RUN_TEST(test_parse_reply_with_leading_glitch_byte);
    RUN_TEST(test_parse_reply_without_glitch_byte);
    RUN_TEST(test_parse_incomplete_until_full_frame);
    RUN_TEST(test_parse_bad_crc);
    RUN_TEST(test_parse_exception);
    RUN_TEST(test_parse_wrong_server_and_fc);
    RUN_TEST(test_parse_move78_reply_value);
    UNITY_END();
}

void loop() {}
