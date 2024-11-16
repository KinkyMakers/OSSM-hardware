#ifndef OSSM_SOFTWARE_COMMAND_STREAM_H
#define OSSM_SOFTWARE_COMMAND_STREAM_H

#pragma once
#include <Arduino.h>

#include <cmath>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

// Command actions are grouped by priority
enum class CommandAction : uint16_t {
    // EMERGENCY (Priority 0)
    E_STOP = 0,

    // SYSTEM (Priority 1)
    RESET_DEVICE = 100,
    STOP_PLAY = 101,
    START_PLAY = 102,

    // CONTROL (Priority 2)
    HOLD_POSITION = 200,
    TRIGGER_HOMING = 201,

    // CONFIGURATION (Priority 3)
    SET_SAFETY_BOUNDS = 300,
    SET_SAFETY_LIMITS = 301,

    // PATTERN (Priority 4)
    SET_PATTERN = 400,

    // PARAMETER (Priority 5)
    SET_PARAMETER = 500,
    STREAM_IN_PARAMETER = 501,

    // TELEMETRY (Priority 6)
    GET_PARAMETER = 600,
    STREAM_OUT_PARAMETER = 601
};

enum class ParameterType : uint8_t {
    // Motion parameters
    POSITION = 0,
    SPEED = 1,
    ACCELERATION = 2,
    DECELERATION = 3,

    // Stroke Engine parameters
    DEPTH = 100,
    SENSATION = 101,
    PATTERN = 102
};

struct CommandPayload {
    ParameterType paramType;
    float value;
    float min;
    float max;
};

struct Command {
    String id;
    CommandAction action;
    CommandPayload payload;
    uint64_t timestamp;

    // Helper to get priority based on CommandType
    int getPriority() const { return static_cast<int>(action); }
};

class CommandStream {
  private:
    // Custom comparator for the priority queue
    struct CommandComparator {
        bool operator()(const std::unique_ptr<Command>& a,
                        const std::unique_ptr<Command>& b) const {
            // Lower priority number = higher priority
            // Return true if a has LOWER priority than b
            return a->getPriority() > b->getPriority();
        }
    };

    std::priority_queue<std::unique_ptr<Command>,
                        std::vector<std::unique_ptr<Command>>,
                        CommandComparator>
        commandQueue;

    std::unique_ptr<Command> currentCommand;

  public:
    CommandStream() = default;

    void enqueue(std::unique_ptr<Command> command) {
        if (command->getPriority() == 0) {
            // If the command has priority 0, clear the queue
            // this priority aligns with emergency stop
            clearQueue();
        }
        commandQueue.push(std::move(command));
    }

    bool isEmpty() const { return commandQueue.empty(); }
    size_t size() const { return commandQueue.size(); }

    const Command* getCurrentCommand() const { return currentCommand.get(); }

    Command* getNext() {
        if (commandQueue.empty()) {
            return nullptr;
        }
        currentCommand = std::move(
            const_cast<std::unique_ptr<Command>&>(commandQueue.top())
        );
        commandQueue.pop();
        return currentCommand.get();
    }

    void clearQueue() {
        while (!commandQueue.empty()) {
            commandQueue.pop();
        }
    }
};

#endif  // OSSM_SOFTWARE_COMMAND_STREAM_H
