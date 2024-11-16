#include <memory>

#include "command/CommandStream.hpp"
#include "unity.h"

CommandStream stream;

void setUp(void) { stream = CommandStream(); }

void tearDown(void) {}

void test_PriorityQueueOrder(void) {
    // Create commands with different priorities using the proper Command
    // structure
    auto cmd1 = std::make_unique<Command>(Command{
        .id = "low_priority",
        .action = CommandAction::SET_PARAMETER,  // Priority 5
        .payload = {.paramType = ParameterType::POSITION, .value = 100.0f},
        .timestamp = 0});

    auto cmd2 = std::make_unique<Command>(Command{
        .id = "high_priority",
        .action = CommandAction::STOP_PLAY,  // Priority 1
        .payload = {.paramType = ParameterType::POSITION, .value = 200.0f},
        .timestamp = 0});

    auto cmd3 = std::make_unique<Command>(Command{
        .id = "medium_priority",
        .action = CommandAction::HOLD_POSITION,  // Priority 2
        .payload = {.paramType = ParameterType::POSITION, .value = 300.0f},
        .timestamp = 0});

    // Add commands in random order
    stream.enqueue(std::move(cmd1));
    stream.enqueue(std::move(cmd2));
    stream.enqueue(std::move(cmd3));

    // Verify queue size
    TEST_ASSERT_EQUAL(3, stream.size());

    // Test command processing order
    Command* cmd = stream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::STOP_PLAY),
                      static_cast<int>(cmd->action));
    TEST_ASSERT_EQUAL(2, stream.size());

    cmd = stream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::HOLD_POSITION),
                      static_cast<int>(cmd->action));
    TEST_ASSERT_EQUAL(1, stream.size());

    cmd = stream.getNext();
    TEST_ASSERT_NOT_NULL(cmd);
    TEST_ASSERT_EQUAL(static_cast<int>(CommandAction::SET_PARAMETER),
                      static_cast<int>(cmd->action));
    TEST_ASSERT_EQUAL(0, stream.size());
}

void test_EmptyQueueBehavior(void) {
    TEST_ASSERT_TRUE(stream.isEmpty());
    TEST_ASSERT_EQUAL(0, stream.size());

    // Test getting command from empty queue
    Command* cmd = stream.getNext();
    TEST_ASSERT_NULL(cmd);
}

int runUnityTests(void) {
    UNITY_BEGIN();
    RUN_TEST(test_PriorityQueueOrder);
    RUN_TEST(test_EmptyQueueBehavior);
    return UNITY_END();
}

int main(void) { return runUnityTests(); }
