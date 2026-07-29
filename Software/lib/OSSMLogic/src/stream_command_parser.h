#ifndef OSSM_STREAM_COMMAND_PARSER_H
#define OSSM_STREAM_COMMAND_PARSER_H

#include <cstddef>
#include <cstdint>

namespace stream_command_parser {

    struct Command {
        uint8_t position = 0;
        uint16_t durationMilliseconds = 0;
    };

    inline bool parse(const char *data, size_t length, Command &result) {
        constexpr char prefix[] = "stream:";
        constexpr size_t prefixLength = sizeof(prefix) - 1;
        if (data == nullptr || length <= prefixLength) return false;
        for (size_t index = 0; index < prefixLength; ++index) {
            if (data[index] != prefix[index]) return false;
        }

        size_t index = prefixLength;
        uint32_t position = 0;
        size_t positionDigits = 0;
        while (index < length && data[index] != ':') {
            if (data[index] < '0' || data[index] > '9') return false;
            position = position * 10 + static_cast<uint32_t>(data[index] - '0');
            if (position > 100) return false;
            ++positionDigits;
            ++index;
        }
        if (positionDigits == 0 || index >= length || data[index] != ':')
            return false;
        ++index;

        uint32_t duration = 0;
        size_t durationDigits = 0;
        while (index < length) {
            if (data[index] < '0' || data[index] > '9') return false;
            duration = duration * 10 + static_cast<uint32_t>(data[index] - '0');
            if (duration > UINT16_MAX) return false;
            ++durationDigits;
            ++index;
        }
        if (durationDigits == 0) return false;

        result.position = static_cast<uint8_t>(position);
        result.durationMilliseconds = static_cast<uint16_t>(duration);
        return true;
    }

}  // namespace stream_command_parser

#endif  // OSSM_STREAM_COMMAND_PARSER_H
