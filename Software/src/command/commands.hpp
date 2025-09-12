#ifndef OSSM_SOFTWARE_COMMANDS_H
#define OSSM_SOFTWARE_COMMANDS_H

#include <regex>
#include <string>

#include "Arduino.h"

// These are BLE commands that we will process and send to the state machine.
// The state machine will execute these commands if appropiate

namespace Prefix {
    const char goTo[] PROGMEM = "go:";
    const char setValue[] PROGMEM = "set:";
}

enum class Commands {
    // GO TO
    goToStrokeEngine,
    goToSimplePenetration,
    goToStreaming,
    goToMenu,

    // SET VALUES
    setDepth,
    setSensation,
    setPattern,
    setSpeed,
    setStroke,

    ignore
};

struct CommandValue {
    Commands command;
    int value;
};

inline CommandValue setCommandValue(const String& str) {
    // Check if string starts with "set:" and has two colons
    int firstColon = str.indexOf(':');
    int lastColon = str.lastIndexOf(':');
    if (firstColon == -1 || lastColon == -1 || firstColon == lastColon) {
        ESP_LOGI("COMMANDS", "Command not well formed: %s", str.c_str());
        return {Commands::ignore, 0};
    }

    // Get value after last colon and validate it's a number between 0-100
    String command =
        str.substring(4, str.lastIndexOf(':'));  // Skip "set:" and get command
    String valueStr = str.substring(lastColon + 1);
    int value = valueStr.toInt();
    if (value < 0 || value > 100 || valueStr != String(value)) {
        ESP_LOGI("COMMANDS", "Invalid value: %s", str.c_str());
        return {Commands::ignore, 0};
    }

    if (command == "depth") {
        return {Commands::setDepth, value};
    } else if (command == "sensation") {
        return {Commands::setSensation, value};
    } else if (command == "pattern") {
        return {Commands::setPattern, value};
    } else if (command == "speed") {
        return {Commands::setSpeed, value};
    } else if (command == "stroke") {
        return {Commands::setStroke, value};
    } else {
        return {Commands::ignore, 0};
    }
}

inline CommandValue commandFromString(const String& str) {
    // Create temporary strings from PROGMEM for comparison
    String goToPrefix = String(FPSTR(Prefix::goTo));
    String setValuePrefix = String(FPSTR(Prefix::setValue));

    if (str.startsWith(goToPrefix)) {
        if (str == goToPrefix + "strokeEngine")
            return {Commands::goToStrokeEngine, 0};
        if (str == goToPrefix + "simplePenetration")
            return {Commands::goToSimplePenetration, 0};
        if (str == goToPrefix + "streaming")
            return {Commands::goToStreaming, 0};
        if (str == goToPrefix + "menu") return {Commands::goToMenu, 0};
        return {Commands::goToMenu, 0};  // Default
    }

    if (str.startsWith(setValuePrefix)) {
        return setCommandValue(str);
    }

    return {Commands::ignore, 0};
}

#endif  // OSSM_SOFTWARE_COMMANDS_H
