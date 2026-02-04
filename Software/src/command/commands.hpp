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

    // STREAMING
    streamPosition,

    ignore
};

struct CommandValue {
    Commands command;
    int value;
    int time;  // Used for streaming commands (time in ms)
};

inline CommandValue setCommandValue(const String& str) {
    // Check if string starts with "set:" and has two colons
    int firstColon = str.indexOf(':');
    int lastColon = str.lastIndexOf(':');
    if (firstColon == -1 || lastColon == -1 || firstColon == lastColon) {
        ESP_LOGI("COMMANDS", "Command not well formed: %s", str.c_str());
        return {Commands::ignore, 0, 0};
    }

    // Get value after last colon and validate it's a number between 0-100
    String command =
        str.substring(4, str.lastIndexOf(':'));  // Skip "set:" and get command
    String valueStr = str.substring(lastColon + 1);
    int value = valueStr.toInt();
    if (value < 0 || value > 100 || valueStr != String(value)) {
        ESP_LOGI("COMMANDS", "Invalid value: %s", str.c_str());
        return {Commands::ignore, 0, 0};
    }

    if (command == "depth") {
        return {Commands::setDepth, value, 0};
    } else if (command == "sensation") {
        return {Commands::setSensation, value, 0};
    } else if (command == "pattern") {
        return {Commands::setPattern, value, 0};
    } else if (command == "speed") {
        return {Commands::setSpeed, value, 0};
    } else if (command == "stroke") {
        return {Commands::setStroke, value, 0};
    } else {
        return {Commands::ignore, 0, 0};
    }
}

inline CommandValue streamCommandValue(const String& str) {
    // Format: stream:pos:time
    // pos = 0-100 (position percentage)
    // time = milliseconds to reach position
    int firstColon = str.indexOf(':');
    int lastColon = str.lastIndexOf(':');
    if (firstColon == -1 || lastColon == -1 || firstColon == lastColon) {
        ESP_LOGI("COMMANDS", "Stream command not well formed: %s", str.c_str());
        return {Commands::ignore, 0, 0};
    }

    // Extract position (between first and last colon)
    String posStr = str.substring(firstColon + 1, lastColon);
    int pos = posStr.toInt();
    if (pos < 0 || pos > 100) {
        ESP_LOGI("COMMANDS", "Invalid stream position: %s", str.c_str());
        return {Commands::ignore, 0, 0};
    }

    // Extract time (after last colon)
    String timeStr = str.substring(lastColon + 1);
    int time = timeStr.toInt();
    if (time < 0) {
        ESP_LOGI("COMMANDS", "Invalid stream time: %s", str.c_str());
        return {Commands::ignore, 0, 0};
    }

    return {Commands::streamPosition, pos, time};
}

static const char ignore_str[] PROGMEM = "ignore";

inline CommandValue commandFromString(const String& str) {
    if (str.startsWith("go:")) {
        if (str == "go:strokeEngine") return {Commands::goToStrokeEngine, 0, 0};
        if (str == "go:simplePenetration")
            return {Commands::goToSimplePenetration, 0, 0};
        if (str == "go:streaming") return {Commands::goToStreaming, 0, 0};
        if (str == "go:menu") return {Commands::goToMenu, 0, 0};
        return {Commands::goToMenu, 0, 0};  // Default
    }

    if (str.startsWith("set:")) {
        return setCommandValue(str);
    }

    if (str.startsWith("stream:")) {
        return streamCommandValue(str);
    }

    return {Commands::ignore, 0, 0};
}

#endif  // OSSM_SOFTWARE_COMMANDS_H
