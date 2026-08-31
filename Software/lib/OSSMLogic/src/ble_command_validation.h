#pragma once

#include <string_view>

// Syntax checks for the established text BLE protocol. Keep these independent
// of numeric conversion, range validation and command dispatch.
namespace ble_command_validation {

namespace detail {

constexpr bool isDigit(char value) {
    return value >= '0' && value <= '9';
}

constexpr bool isWhitespace(char value) {
    return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
           value == '\f' || value == '\v';
}

inline bool consumePrefix(std::string_view& value, std::string_view prefix) {
    if (value.substr(0, prefix.size()) != prefix) return false;
    value.remove_prefix(prefix.size());
    return true;
}

inline bool isDecimal(std::string_view value) {
    if (value.empty()) return false;
    for (char character : value) {
        if (!isDigit(character)) return false;
    }
    return true;
}

inline bool equalsIgnoreCase(std::string_view value, std::string_view lower) {
    if (value.size() != lower.size()) return false;
    for (std::size_t index = 0; index < value.size(); ++index) {
        char character = value[index];
        if (character >= 'A' && character <= 'Z') character += 'a' - 'A';
        if (character != lower[index]) return false;
    }
    return true;
}

}  // namespace detail

inline bool isLegacyCommand(std::string_view command) {
    if (detail::consumePrefix(command, "go:")) {
        return command == "simplePenetration" || command == "strokeEngine" ||
               command == "streaming" || command == "menu";
    }

    if (detail::consumePrefix(command, "set:")) {
        if (detail::consumePrefix(command, "wifi:")) {
            const auto separator = command.find('|');
            if (separator == std::string_view::npos || separator == 0 ||
                separator + 1 == command.size()) {
                return false;
            }
            // The old wildcard permits further pipes and NUL bytes in the
            // password, but excludes CR/LF. The SSID permits either newline.
            return command.substr(separator + 1).find_first_of("\r\n") ==
                   std::string_view::npos;
        }

        const auto separator = command.find(':');
        if (separator == std::string_view::npos) return false;
        const auto parameter = command.substr(0, separator);
        if (parameter != "speed" && parameter != "stroke" &&
            parameter != "depth" && parameter != "sensation" &&
            parameter != "buffer" && parameter != "pattern") {
            return false;
        }
        return detail::isDecimal(command.substr(separator + 1));
    }

    if (detail::consumePrefix(command, "stream:")) {
        const auto separator = command.find(':');
        return separator != std::string_view::npos &&
               detail::isDecimal(command.substr(0, separator)) &&
               detail::isDecimal(command.substr(separator + 1));
    }

    return false;
}

inline bool isGpioCommand(std::string_view command) {
    while (!command.empty() && detail::isWhitespace(command.front())) {
        command.remove_prefix(1);
    }

    std::size_t digits = 0;
    while (digits < command.size() && detail::isDigit(command[digits])) {
        ++digits;
    }
    if (digits == 0) return false;
    command.remove_prefix(digits);

    while (!command.empty() && detail::isWhitespace(command.front())) {
        command.remove_prefix(1);
    }
    if (!detail::consumePrefix(command, ":")) return false;

    while (!command.empty() && detail::isWhitespace(command.back())) {
        command.remove_suffix(1);
    }
    return command == "0" || command == "1" ||
           detail::equalsIgnoreCase(command, "low") ||
           detail::equalsIgnoreCase(command, "high");
}

}  // namespace ble_command_validation
