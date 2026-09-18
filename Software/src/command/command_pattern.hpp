#ifndef OSSM_COMMAND_PATTERN_HPP
#define OSSM_COMMAND_PATTERN_HPP

#include <regex>
#include <string>

// Single source of truth for the BLE command grammar. The NimBLE write
// callback validates against this and the native tests exercise it, so the
// accepted set cannot drift from commandFromString().
inline const char* bleCommandPattern() {
    return R"(go:(simplePenetration|strokeEngine|streaming|menu|restart|update|pairing)|set:(speed|stroke|depth|sensation|buffer|pattern):\d+|set:wifi:[^|]+\|.+|stream:\d+:\d+)";
}

inline bool isValidBleCommand(const std::string& command) {
    static const std::regex pattern(bleCommandPattern());
    return std::regex_match(command, pattern);
}

#endif  // OSSM_COMMAND_PATTERN_HPP
