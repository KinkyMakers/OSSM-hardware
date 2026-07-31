#ifndef OSSM_STREAM_COMMAND_PARSER_H
#define OSSM_STREAM_COMMAND_PARSER_H

#include <cstddef>
#include <cstdint>

namespace stream_command_parser {

    struct Command {
        uint8_t position = 0;
        uint16_t durationMilliseconds = 0;
    };

    constexpr uint8_t packedMagic0 = 0xA5;
    constexpr uint8_t packedMagic1 = 0x5A;
    constexpr uint8_t packedVersion = 1;
    constexpr size_t packedHeaderLength = 4;
    constexpr size_t packedCommandLength = 3;
    constexpr size_t maximumPackedCommands = 16;

    struct PackedCommands {
        Command commands[maximumPackedCommands]{};
        size_t count = 0;
    };

    inline bool hasPackedPrefix(const uint8_t *data, size_t length) {
        return data != nullptr && length >= 2 && data[0] == packedMagic0 &&
               data[1] == packedMagic1;
    }

    inline bool parsePacked(const uint8_t *data, size_t length,
                            PackedCommands &result) {
        result = {};
        if (!hasPackedPrefix(data, length) || length < packedHeaderLength ||
            data[2] != packedVersion)
            return false;

        const size_t count = data[3];
        if (count == 0 || count > maximumPackedCommands ||
            length != packedHeaderLength + count * packedCommandLength)
            return false;

        for (size_t commandIndex = 0; commandIndex < count; ++commandIndex) {
            const size_t offset =
                packedHeaderLength + commandIndex * packedCommandLength;
            const uint8_t position = data[offset];
            const uint16_t duration =
                static_cast<uint16_t>(data[offset + 1]) |
                static_cast<uint16_t>(data[offset + 2]) << 8;
            if (position > 100) return false;
            result.commands[commandIndex] = {position, duration};
        }
        result.count = count;
        return true;
    }

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
