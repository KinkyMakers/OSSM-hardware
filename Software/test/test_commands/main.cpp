#include <ArduinoFake.h>
#include <unity.h>

// Stub ESP-IDF logging macros (not available on native platform)
#define ESP_LOGI(tag, fmt, ...)
#define ESP_LOGW(tag, fmt, ...)
#define ESP_LOGE(tag, fmt, ...)

#include "command/commands.hpp"

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

void test_commandFromString_goRestart() {
    auto result = commandFromString(String("go:restart"));
    TEST_ASSERT_EQUAL(Commands::goToRestart, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
}

void test_commandFromString_goUpdate() {
    auto result = commandFromString(String("go:update"));
    TEST_ASSERT_EQUAL(Commands::goToUpdate, result.command);
    TEST_ASSERT_EQUAL(0, result.value);
    TEST_ASSERT_EQUAL(0, result.time);
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

int runUnityTests() {
    UNITY_BEGIN();

    // commandFromString
    RUN_TEST(test_commandFromString_goStrokeEngine);
    RUN_TEST(test_commandFromString_goSimplePenetration);
    RUN_TEST(test_commandFromString_goStreaming);
    RUN_TEST(test_commandFromString_goMenu);
    RUN_TEST(test_commandFromString_goRestart);
    RUN_TEST(test_commandFromString_goUpdate);
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

    // parseWiFiCommand
    RUN_TEST(test_parseWiFiCommand_valid);
    RUN_TEST(test_parseWiFiCommand_noPipe_returnsEmpty);
    RUN_TEST(test_parseWiFiCommand_wrongPrefix_returnsEmpty);

    return UNITY_END();
}

int main(void) { return runUnityTests(); }
