#include <ArduinoFake.h>
#include <unity.h>

// Stub ESP-IDF logging macros (not available on native platform)
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)

#include "command/commands.hpp"
#include "stream_command_parser.h"

// ---------------------------------------------------------------------------
// commandFromString tests
// ---------------------------------------------------------------------------

void test_commandFromString_goStrokeEngine() {
    auto result = commandFromString(String("go:strokeEngine"));
    TEST_ASSERT_EQUAL(Commands::goToStrokeEngine, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
}

void test_commandFromString_goSimplePenetration() {
    auto result = commandFromString(String("go:simplePenetration"));
    TEST_ASSERT_EQUAL(Commands::goToSimplePenetration, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
}

void test_commandFromString_goStreaming() {
    auto result = commandFromString(String("go:streaming"));
    TEST_ASSERT_EQUAL(Commands::goToStreaming, result.command);
}

void test_commandFromString_goMenu() {
    auto result = commandFromString(String("go:menu"));
    TEST_ASSERT_EQUAL(Commands::goToMenu, result.command);
}

void test_commandFromString_goUnknown_defaultsToMenu() {
    auto result = commandFromString(String("go:unknown"));
    TEST_ASSERT_EQUAL(Commands::goToMenu, result.command);
}

void test_commandFromString_garbage_returnsIgnore() {
    auto result = commandFromString(String("garbage"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

// ---------------------------------------------------------------------------
// setCommandValue tests
// ---------------------------------------------------------------------------

void test_setCommandValue_speed50() {
    auto result = setCommandValue(String("set:speed:50"));
    TEST_ASSERT_EQUAL(Commands::setSpeed, result.command);
    TEST_ASSERT_EQUAL(50, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
}

void test_setCommandValue_depth0_boundary() {
    auto result = setCommandValue(String("set:depth:0"));
    TEST_ASSERT_EQUAL(Commands::setDepth, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
}

void test_setCommandValue_depth100_boundary() {
    auto result = setCommandValue(String("set:depth:100"));
    TEST_ASSERT_EQUAL(Commands::setDepth, result.command);
    TEST_ASSERT_EQUAL(100, result.value);
}

void test_setCommandValue_speedNegative_returnsIgnore() {
    auto result = setCommandValue(String("set:speed:-1"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_setCommandValue_speed101_returnsIgnore() {
    auto result = setCommandValue(String("set:speed:101"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_setCommandValue_speedAbc_returnsIgnore() {
    auto result = setCommandValue(String("set:speed:abc"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_setCommandValue_unknownParam_returnsIgnore() {
    auto result = setCommandValue(String("set:unknown:50"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_setCommandValue_malformedSingleColon_returnsIgnore() {
    auto result = setCommandValue(String("set:speed"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

// ---------------------------------------------------------------------------
// streamCommandValue tests
// ---------------------------------------------------------------------------

void test_streamCommandValue_valid() {
    auto result = streamCommandValue(String("stream:50:200"));
    TEST_ASSERT_EQUAL(Commands::streamPosition, result.command);
    TEST_ASSERT_EQUAL(50, result.value);
    TEST_ASSERT_EQUAL(200, result.time);
}

void test_streamCommandValue_zeroBoundary() {
    auto result = streamCommandValue(String("stream:0:0"));
    TEST_ASSERT_EQUAL(Commands::streamPosition, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
}

void test_streamCommandValue_pos101_returnsIgnore() {
    auto result = streamCommandValue(String("stream:101:100"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_streamCommandValue_posNegative_returnsIgnore() {
    auto result = streamCommandValue(String("stream:-1:100"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_streamCommandValue_malformedSingleColon_returnsIgnore() {
    auto result = streamCommandValue(String("stream:50"));
    TEST_ASSERT_EQUAL(Commands::ignore, result.command);
}

void test_streamFastParser_accepts_bounded_wire_command() {
    stream_command_parser::Command command;
    const char wire[] = "stream:80:65535";
    TEST_ASSERT_TRUE(stream_command_parser::parse(
        wire, sizeof(wire) - 1, command));
    TEST_ASSERT_EQUAL_UINT8(80, command.position);
    TEST_ASSERT_EQUAL_UINT16(65535, command.durationMilliseconds);
}

void test_streamFastParser_rejects_out_of_range_values() {
    stream_command_parser::Command command;
    const char position[] = "stream:101:20";
    const char duration[] = "stream:50:65536";
    TEST_ASSERT_FALSE(stream_command_parser::parse(
        position, sizeof(position) - 1, command));
    TEST_ASSERT_FALSE(stream_command_parser::parse(
        duration, sizeof(duration) - 1, command));
}

void test_streamFastParser_rejects_trailing_or_missing_data() {
    stream_command_parser::Command command;
    const char trailing[] = "stream:50:20x";
    const char missingDuration[] = "stream:50:";
    TEST_ASSERT_FALSE(stream_command_parser::parse(
        trailing, sizeof(trailing) - 1, command));
    TEST_ASSERT_FALSE(stream_command_parser::parse(
        missingDuration, sizeof(missingDuration) - 1, command));
}

void test_streamPackedParser_accepts_ordered_waypoints() {
    const uint8_t wire[] = {
        stream_command_parser::packedMagic0,
        stream_command_parser::packedMagic1,
        stream_command_parser::packedVersion,
        3,
        20, 25, 0,
        80, 50, 0,
        100, 0xFF, 0xFF,
    };
    stream_command_parser::PackedCommands packed;
    TEST_ASSERT_TRUE(stream_command_parser::parsePacked(
        wire, sizeof(wire), packed));
    TEST_ASSERT_EQUAL_UINT32(3, packed.count);
    TEST_ASSERT_EQUAL_UINT8(20, packed.commands[0].position);
    TEST_ASSERT_EQUAL_UINT16(25, packed.commands[0].durationMilliseconds);
    TEST_ASSERT_EQUAL_UINT8(80, packed.commands[1].position);
    TEST_ASSERT_EQUAL_UINT16(50, packed.commands[1].durationMilliseconds);
    TEST_ASSERT_EQUAL_UINT8(100, packed.commands[2].position);
    TEST_ASSERT_EQUAL_UINT16(65535, packed.commands[2].durationMilliseconds);
}

void test_streamPackedParser_rejects_invalid_payloads_atomically() {
    const uint8_t wrongLength[] = {
        stream_command_parser::packedMagic0,
        stream_command_parser::packedMagic1,
        stream_command_parser::packedVersion,
        2,
        50, 25, 0,
    };
    const uint8_t invalidPosition[] = {
        stream_command_parser::packedMagic0,
        stream_command_parser::packedMagic1,
        stream_command_parser::packedVersion,
        1,
        101, 25, 0,
    };
    stream_command_parser::PackedCommands packed;
    TEST_ASSERT_FALSE(stream_command_parser::parsePacked(
        wrongLength, sizeof(wrongLength), packed));
    TEST_ASSERT_FALSE(stream_command_parser::parsePacked(
        invalidPosition, sizeof(invalidPosition), packed));
}

// ---------------------------------------------------------------------------
// parseWiFiCommand tests
// ---------------------------------------------------------------------------

void test_parseWiFiCommand_valid() {
    auto result = parseWiFiCommand(String("set:wifi:MySSID|MyPass"));
    TEST_ASSERT_EQUAL_STRING("MySSID", result.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("MyPass", result.password.c_str());
}

void test_parseWiFiCommand_noPipe_returnsEmpty() {
    auto result = parseWiFiCommand(String("set:wifi:NoPipe"));
    TEST_ASSERT_EQUAL_STRING("", result.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", result.password.c_str());
}

void test_parseWiFiCommand_wrongPrefix_returnsEmpty() {
    auto result = parseWiFiCommand(String("not:wifi"));
    TEST_ASSERT_EQUAL_STRING("", result.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("", result.password.c_str());
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // commandFromString
    RUN_TEST(test_commandFromString_goStrokeEngine);
    RUN_TEST(test_commandFromString_goSimplePenetration);
    RUN_TEST(test_commandFromString_goStreaming);
    RUN_TEST(test_commandFromString_goMenu);
    RUN_TEST(test_commandFromString_goUnknown_defaultsToMenu);
    RUN_TEST(test_commandFromString_garbage_returnsIgnore);

    // setCommandValue
    RUN_TEST(test_setCommandValue_speed50);
    RUN_TEST(test_setCommandValue_depth0_boundary);
    RUN_TEST(test_setCommandValue_depth100_boundary);
    RUN_TEST(test_setCommandValue_speedNegative_returnsIgnore);
    RUN_TEST(test_setCommandValue_speed101_returnsIgnore);
    RUN_TEST(test_setCommandValue_speedAbc_returnsIgnore);
    RUN_TEST(test_setCommandValue_unknownParam_returnsIgnore);
    RUN_TEST(test_setCommandValue_malformedSingleColon_returnsIgnore);

    // streamCommandValue
    RUN_TEST(test_streamCommandValue_valid);
    RUN_TEST(test_streamCommandValue_zeroBoundary);
    RUN_TEST(test_streamCommandValue_pos101_returnsIgnore);
    RUN_TEST(test_streamCommandValue_posNegative_returnsIgnore);
    RUN_TEST(test_streamCommandValue_malformedSingleColon_returnsIgnore);
    RUN_TEST(test_streamFastParser_accepts_bounded_wire_command);
    RUN_TEST(test_streamFastParser_rejects_out_of_range_values);
    RUN_TEST(test_streamFastParser_rejects_trailing_or_missing_data);
    RUN_TEST(test_streamPackedParser_accepts_ordered_waypoints);
    RUN_TEST(test_streamPackedParser_rejects_invalid_payloads_atomically);

    // parseWiFiCommand
    RUN_TEST(test_parseWiFiCommand_valid);
    RUN_TEST(test_parseWiFiCommand_noPipe_returnsEmpty);
    RUN_TEST(test_parseWiFiCommand_wrongPrefix_returnsEmpty);

    return UNITY_END();
}
