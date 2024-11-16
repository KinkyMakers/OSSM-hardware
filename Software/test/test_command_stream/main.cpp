#include <memory>

#include "command/CommandStream.cpp"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_PriorityQueueOrder(void) {
    // All these commands have priority >= 400, so no clearing should occur
    auto cmd1 = std::make_unique<Command>(Command{
        .id = "low_priority",
        .action = CommandAction::SET_PARAMETER,  // Priority 500
        .payload = {.paramType = ParameterType::POSITION, .value = 100.0f},
        .timestamp = 0});

    auto cmd2 = std::make_unique<Command>(Command{
        .id = "medium_priority",
        .action = CommandAction::SET_PATTERN,  // Priority 400
        .payload = {.paramType = ParameterType::PATTERN, .value = 200.0f},
        .timestamp = 0});

    auto cmd3 = std::make_unique<Command>(Command{
        .id = "lowest_priority",
        .action = CommandAction::GET_PARAMETER,  // Priority 600
        .payload = {.paramType = ParameterType::POSITION, .value = 300.0f},
        .timestamp = 0});

    // Add commands and verify they're all preserved
    commandStream.enqueue(std::move(cmd1));
    TEST_ASSERT_EQUAL(1, commandStream.size());
    commandStream.enqueue(std::move(cmd2));
    TEST_ASSERT_EQUAL(2, commandStream.size());
    commandStream.enqueue(std::move(cmd3));
    TEST_ASSERT_EQUAL(3, commandStream.size());

    // Verify processing order (highest to lowest priority)
    Command* cmd = commandStream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::SET_PATTERN),
                      static_cast<int>(cmd->action));

    cmd = commandStream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::SET_PARAMETER),
                      static_cast<int>(cmd->action));

    cmd = commandStream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::GET_PARAMETER),
                      static_cast<int>(cmd->action));
}

void test_EmptyQueueBehavior(void) {
    TEST_ASSERT_TRUE(commandStream.isEmpty());
    TEST_ASSERT_EQUAL(0, commandStream.size());

    // Test getting command from empty queue
    Command* cmd = commandStream.getNext();
    TEST_ASSERT_NULL(cmd);
}

void test_PriorityUnseating(void) {
    // Test that system commands (priority < 400) clear higher priority commands

    // Fill queue with higher priority commands first
    auto cmd1 = std::make_unique<Command>(Command{
        .id = "pattern_cmd",
        .action = CommandAction::SET_PATTERN,  // Priority 400
        .payload = {.paramType = ParameterType::PATTERN, .value = 100.0f},
        .timestamp = 0});

    auto cmd2 = std::make_unique<Command>(Command{
        .id = "param_cmd",
        .action = CommandAction::SET_PARAMETER,  // Priority 500
        .payload = {.paramType = ParameterType::POSITION, .value = 200.0f},
        .timestamp = 0});

    commandStream.enqueue(std::move(cmd1));
    commandStream.enqueue(std::move(cmd2));
    TEST_ASSERT_EQUAL(2, commandStream.size());

    // Add a system command that should clear the queue
    auto system_cmd = std::make_unique<Command>(Command{
        .id = "system_cmd",
        .action = CommandAction::STOP_PLAY,  // Priority 101
        .payload = {.paramType = ParameterType::POSITION, .value = 0.0f},
        .timestamp = 0});

    commandStream.enqueue(std::move(system_cmd));

    // Verify queue now only contains system command
    TEST_ASSERT_EQUAL(1, commandStream.size());
    Command* cmd = commandStream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::STOP_PLAY),
                      static_cast<int>(cmd->action));
    TEST_ASSERT_EQUAL_STRING("system_cmd", cmd->id.c_str());
}

void test_MultiLevelClearing(void) {
    // Test that different priority levels clear appropriately

    // Add some high priority commands
    auto cmd1 = std::make_unique<Command>(Command{
        .id = "pattern_cmd",
        .action = CommandAction::SET_PATTERN,  // Priority 400
        .payload = {.paramType = ParameterType::PATTERN, .value = 100.0f},
        .timestamp = 0});

    commandStream.enqueue(std::move(cmd1));
    TEST_ASSERT_EQUAL(1, commandStream.size());

    // Add control command (priority 200) - should clear pattern command
    auto control_cmd = std::make_unique<Command>(Command{
        .id = "hold_pos",
        .action = CommandAction::HOLD_POSITION,  // Priority 200
        .payload = {.paramType = ParameterType::POSITION, .value = 0.0f},
        .timestamp = 0});

    commandStream.enqueue(std::move(control_cmd));
    TEST_ASSERT_EQUAL(1, commandStream.size());

    // Emergency command should clear everything
    auto emergency_cmd = std::make_unique<Command>(Command{
        .id = "emergency",
        .action = CommandAction::EMERGENCY_STOP,  // Priority 0
        .payload = {.paramType = ParameterType::POSITION, .value = 0.0f},
        .timestamp = 0});

    commandStream.enqueue(std::move(emergency_cmd));
    TEST_ASSERT_EQUAL(1, commandStream.size());

    Command* cmd = commandStream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::EMERGENCY_STOP),
                      static_cast<int>(cmd->action));
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_PriorityQueueOrder);
    RUN_TEST(test_EmptyQueueBehavior);
    RUN_TEST(test_PriorityUnseating);
    RUN_TEST(test_MultiLevelClearing);
    return UNITY_END();
}

int main(void) { return runUnityTests(); }
