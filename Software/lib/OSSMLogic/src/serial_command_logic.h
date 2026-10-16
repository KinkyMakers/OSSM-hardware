#pragma once

#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <cstdlib>

// Line parser for the USB settings console. No Arduino types.

namespace serial_command_logic {

enum class Verb {
    Empty,
    Help,
    Get,
    Info,
    Set,
    Name,
    Restart,
    Json,
    Unknown,
};

inline constexpr size_t kMaxKey = 40;
inline constexpr size_t kMaxValue = 256;

struct ParsedCommand {
    Verb verb = Verb::Empty;
    char key[kMaxKey]{};
    char value[kMaxValue]{};
    bool hasKey = false;
    bool hasValue = false;
};

inline void trimInPlace(char *text) {
    if (text == nullptr) return;
    size_t start = 0;
    while (text[start] != '\0' &&
           std::isspace(static_cast<unsigned char>(text[start])))
        ++start;
    size_t end = std::strlen(text);
    while (end > start &&
           std::isspace(static_cast<unsigned char>(text[end - 1])))
        --end;
    const size_t length = end - start;
    if (start > 0 && length > 0) std::memmove(text, text + start, length);
    text[length] = '\0';
}

inline void copyBounded(char *dest, size_t destSize, const char *src,
                        size_t srcLength) {
    if (destSize == 0) return;
    if (srcLength >= destSize) srcLength = destSize - 1;
    std::memcpy(dest, src, srcLength);
    dest[srcLength] = '\0';
}

inline void stripMatchingQuotes(char *text) {
    const size_t length = std::strlen(text);
    if (length >= 2 &&
        ((text[0] == '"' && text[length - 1] == '"') ||
         (text[0] == '\'' && text[length - 1] == '\''))) {
        std::memmove(text, text + 1, length - 2);
        text[length - 2] = '\0';
    }
}

inline Verb verbFromWord(const char *word) {
    if (std::strcmp(word, "help") == 0 || std::strcmp(word, "?") == 0)
        return Verb::Help;
    if (std::strcmp(word, "get") == 0) return Verb::Get;
    if (std::strcmp(word, "info") == 0) return Verb::Info;
    if (std::strcmp(word, "set") == 0) return Verb::Set;
    if (std::strcmp(word, "name") == 0) return Verb::Name;
    if (std::strcmp(word, "restart") == 0) return Verb::Restart;
    return Verb::Unknown;
}

inline ParsedCommand parseLine(const char *line) {
    ParsedCommand parsed;
    if (line == nullptr) return parsed;

    char buffer[kMaxValue + kMaxKey + 8];
    copyBounded(buffer, sizeof(buffer), line, std::strlen(line));
    trimInPlace(buffer);
    if (buffer[0] == '\0') return parsed;
    if (buffer[0] == '{') {
        parsed.verb = Verb::Json;
        copyBounded(parsed.value, sizeof(parsed.value), buffer,
                    std::strlen(buffer));
        parsed.hasValue = true;
        return parsed;
    }

    char *cursor = buffer;
    while (*cursor != '\0' &&
           !std::isspace(static_cast<unsigned char>(*cursor))) {
        *cursor = static_cast<char>(
            std::tolower(static_cast<unsigned char>(*cursor)));
        ++cursor;
    }
    const bool hasRest = *cursor != '\0';
    if (hasRest) *cursor++ = '\0';
    parsed.verb = verbFromWord(buffer);

    if (!hasRest) return parsed;
    while (*cursor != '\0' &&
           std::isspace(static_cast<unsigned char>(*cursor)))
        ++cursor;
    if (*cursor == '\0') return parsed;

    if (parsed.verb == Verb::Set) {
        char *keyEnd = cursor;
        while (*keyEnd != '\0' &&
               !std::isspace(static_cast<unsigned char>(*keyEnd)))
            ++keyEnd;
        copyBounded(parsed.key, sizeof(parsed.key), cursor,
                    static_cast<size_t>(keyEnd - cursor));
        parsed.hasKey = parsed.key[0] != '\0';
        cursor = keyEnd;
        while (*cursor != '\0' &&
               std::isspace(static_cast<unsigned char>(*cursor)))
            ++cursor;
        if (*cursor == '\0') return parsed;
        copyBounded(parsed.value, sizeof(parsed.value), cursor,
                    std::strlen(cursor));
        stripMatchingQuotes(parsed.value);
        trimInPlace(parsed.value);
        parsed.hasValue = parsed.value[0] != '\0';
        return parsed;
    }

    copyBounded(parsed.value, sizeof(parsed.value), cursor, std::strlen(cursor));
    stripMatchingQuotes(parsed.value);
    trimInPlace(parsed.value);
    if (parsed.verb == Verb::Get || parsed.verb == Verb::Info) {
        copyBounded(parsed.key, sizeof(parsed.key), parsed.value,
                    std::strlen(parsed.value));
        parsed.hasKey = parsed.key[0] != '\0';
        parsed.hasValue = false;
        parsed.value[0] = '\0';
    } else if (parsed.verb == Verb::Name) {
        parsed.hasValue = parsed.value[0] != '\0';
    }
    return parsed;
}

inline bool parseBool(const char *text, bool &out) {
    if (text == nullptr) return false;
    char buffer[16];
    copyBounded(buffer, sizeof(buffer), text, std::strlen(text));
    trimInPlace(buffer);
    for (char *c = buffer; *c != '\0'; ++c)
        *c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
    if (std::strcmp(buffer, "true") == 0 || std::strcmp(buffer, "1") == 0 ||
        std::strcmp(buffer, "t") == 0 || std::strcmp(buffer, "yes") == 0) {
        out = true;
        return true;
    }
    if (std::strcmp(buffer, "false") == 0 || std::strcmp(buffer, "0") == 0 ||
        std::strcmp(buffer, "f") == 0 || std::strcmp(buffer, "no") == 0) {
        out = false;
        return true;
    }
    return false;
}

inline bool parseNumber(const char *text, float &out) {
    if (text == nullptr) return false;
    char *end = nullptr;
    const float value = std::strtof(text, &end);
    if (end == text || !std::isfinite(value)) return false;
    while (*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
        ++end;
    if (*end != '\0') return false;
    out = value;
    return true;
}

inline bool serialRestartAllowed(const char *state) {
    if (state == nullptr || state[0] == '\0') return false;
    const char *blocked[] = {"homing", "simplePenetration", "strokeEngine",
                             "streaming"};
    for (const char *prefix : blocked) {
        if (std::strncmp(state, prefix, std::strlen(prefix)) == 0) return false;
    }
    return true;
}

}  // namespace serial_command_logic
