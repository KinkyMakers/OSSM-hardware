#ifndef OSSM_SOFTWARE_COMMAND_STREAM_H
#define OSSM_SOFTWARE_COMMAND_STREAM_H

#pragma once
#include <Arduino.h>

#include <cmath>
#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "ossm/OSSMI.h"

// Command actions are grouped by priority
enum class CommandAction : uint16_t {
    // EMERGENCY (Priority 0)
    EMERGENCY_STOP = 0,

    // SYSTEM (Priority 1)
    RESET_DEVICE = 100,
    STOP_PLAY = 101,
    START_PLAY = 102,

    // CONTROL (Priority 2)
    HOLD_POSITION = 200,
    TRIGGER_HOMING = 201,

    // CONFIGURATION (Priority 3)
    SET_SAFETY_BOUNDS = 300,

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

    void clearQueueAbovePriority(uint16_t priority) {
        std::priority_queue<std::unique_ptr<Command>,
                            std::vector<std::unique_ptr<Command>>,
                            CommandComparator>
            tempQueue;

        // Move all commands with priority <= threshold to temp queue
        while (!commandQueue.empty()) {
            if (commandQueue.top()->getPriority() > priority) {
                commandQueue.pop();
            } else {
                tempQueue.push(std::move(
                    const_cast<std::unique_ptr<Command>&>(commandQueue.top())));
                commandQueue.pop();
            }
        }

        // Move remaining commands back
        commandQueue = std::move(tempQueue);
    }

    OSSMInterface* ossm;

  public:
    CommandStream(OSSMInterface* ossm) : ossm(ossm) {}

    void enqueue(std::unique_ptr<Command> command) {
        // Commands with a priority less than 400 will clear all commands above
        // them. These command range 0-399 are associated with safety and system
        // control.
        if (command->getPriority() < 400) {
            clearQueueAbovePriority(command->getPriority());
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
            const_cast<std::unique_ptr<Command>&>(commandQueue.top()));
        commandQueue.pop();
        return currentCommand.get();
    }

    // process current command and get next command
    Command* processCommand() {
        if (currentCommand == nullptr) {
            if (isEmpty()) {
                return nullptr;
            }
            return getNext();
        }

        // visit state machine's current state
        String currentState = ossm->get_current_state();

        // check if current state starts with strokeEngine
        bool isInStrokeEngine = currentState.startsWith("strokeEngine");
        bool isInSimplePenetration =
            currentState.startsWith("simplePenetration");

        // switch on the command type and process
        switch (currentCommand->action) {
            case CommandAction::EMERGENCY_STOP:
                ossm->process_event(emergencyStop);
                break;
            case CommandAction::RESET_DEVICE:
                // restart();

                break;
            case CommandAction::STOP_PLAY:
                if (isInStrokeEngine || isInSimplePenetration) {
                    ossm->process_event(emergencyStop);
                }
                break;
            case CommandAction::START_PLAY:
                // TODO: read the value from the payload and then set the menu
                // option before triggering a button press.
                break;
            case CommandAction::HOLD_POSITION:
                if (isInStrokeEngine || isInSimplePenetration) {
                    // TODO: tell fast accel stepper to hold position.
                    // TOOD: we need to release position as well.
                }
                break;
            case CommandAction::TRIGGER_HOMING:
                ossm->process_event(home);
                break;
            case CommandAction::SET_SAFETY_BOUNDS:
                // TODO immediately set the safety bounds on the OSSM using the
                // current values. This requires some basic filtering.
                break;
            case CommandAction::SET_PATTERN:
                if (isInStrokeEngine) {
                    // TODO: set the stroke engine pattern immediately. as long
                    // as it's valid.
                }
                break;
            case CommandAction::SET_PARAMETER:
                // TODO: set the parameter immediately. as long as it's valid.
                break;
            case CommandAction::STREAM_IN_PARAMETER:
                // TODO: If this is position / time, then send this to the PD
                // controller. Otherwise, ignore.
                break;
            case CommandAction::GET_PARAMETER:
                // TODO: send the current value of the parameter to the
                // client.
                break;
            case CommandAction::STREAM_OUT_PARAMETER:
                // might not be supported.
                break;
            default:
                break;
        }

        return getNext();
    }
};

// Create a global instance
extern CommandStream commandStream;

#endif  // OSSM_SOFTWARE_COMMAND_STREAM_H
