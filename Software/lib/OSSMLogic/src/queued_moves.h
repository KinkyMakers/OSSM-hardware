#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

// Pure logic for the binary "q:" and "r:" BLE commands and their ring buffer
// of queued moves. No hardware dependencies — testable on native platform.
//
// Queued positions are 16-bit: 0-65535 across the streaming stroke window
// (65535 = depth end). On the wire each is one byte, decoded within the
// current range [min, max] as min + (max - min) * byte / 255, so a batch of
// close-together positions keeps fine resolution.
//
// q: 'q' ':' then 2 * n bytes, 0 <= n <= kMaxMovesPerCommand. Each move is one
//    position byte then one time byte (0-255 ms the move lasts before the next
//    one starts). "q:" with no bytes clears the buffer and stops all motion.
// r: 'r' ':' min u16 LE, max u16 LE, then 0 <= n <= kMaxMovesPerRangeCommand
//    moves as in q:. Sets the range and queues the moves together: if they
//    don't fit, neither happens. Later q: moves keep using that range. The
//    range starts at the full 0-65535 each time streaming mode starts.

namespace queued_moves {

constexpr size_t kBufferSize = 32;
constexpr size_t kMaxMovesPerCommand = 10;
// r: carries 4 range bytes, so 8 moves keeps it at 22 bytes, inside one
// default-MTU write like q:.
constexpr size_t kMaxMovesPerRangeCommand = 8;

struct Move {
    uint16_t position;  // 0-65535 across the stroke window
    uint8_t timeMs;
};

/// The 16-bit positions a q:/r: position byte spans: byte 0 is min, 255 max.
struct Range {
    uint16_t min = std::numeric_limits<uint16_t>::min();
    uint16_t max = std::numeric_limits<uint16_t>::max();
};

/// Decode a position byte within `range`, rounding to nearest.
inline uint16_t decodePosition(uint8_t byte, const Range& range) {
    const uint32_t span = static_cast<uint32_t>(range.max) - range.min;
    return static_cast<uint16_t>(range.min + (span * byte + 127) / 255);
}

/// True when the bytes start with the "q:" prefix (whatever follows).
inline bool isQueueCommand(const uint8_t* data, size_t length) {
    return length >= 2 && data[0] == 'q' && data[1] == ':';
}

/// A well-formed "q:" command.
struct QueueCommand {
    size_t count;            // number of moves; 0 means clear and stop
    const uint8_t* payload;  // count * 2 bytes of position/time pairs
};

/// Parse a "q:" command. Empty when the prefix is missing, the payload
/// length is odd, or it holds more than kMaxMovesPerCommand moves.
inline std::optional<QueueCommand> parseQueueCommand(const uint8_t* data,
                                                     size_t length) {
    if (!isQueueCommand(data, length)) {
        return std::nullopt;
    }
    const size_t payloadLength = length - 2;
    if (payloadLength % 2 != 0) {
        return std::nullopt;
    }
    const size_t count = payloadLength / 2;
    if (count > kMaxMovesPerCommand) {
        return std::nullopt;
    }
    return QueueCommand{count, data + 2};
}

/// True when the bytes start with the "r:" prefix (whatever follows).
inline bool isRangeCommand(const uint8_t* data, size_t length) {
    return length >= 2 && data[0] == 'r' && data[1] == ':';
}

/// A well-formed "r:" command.
struct RangeCommand {
    Range range;
    size_t count;            // number of moves; 0 just sets the range
    const uint8_t* payload;  // count * 2 bytes of position/time pairs
};

/// Parse an "r:" command. Empty when the prefix or range is missing, the
/// moves aren't whole pairs, there are more than kMaxMovesPerRangeCommand,
/// or min > max.
inline std::optional<RangeCommand> parseRangeCommand(const uint8_t* data,
                                                     size_t length) {
    if (!isRangeCommand(data, length) || length < 6) {
        return std::nullopt;
    }
    const Range range{static_cast<uint16_t>(data[2] | (data[3] << 8)),
                      static_cast<uint16_t>(data[4] | (data[5] << 8))};
    if (range.min > range.max) {
        return std::nullopt;
    }
    const size_t payloadLength = length - 6;
    if (payloadLength % 2 != 0) {
        return std::nullopt;
    }
    const size_t count = payloadLength / 2;
    if (count > kMaxMovesPerRangeCommand) {
        return std::nullopt;
    }
    return RangeCommand{range, count, data + 6};
}

/// Fixed-size ring buffer of moves. Not thread safe: callers lock.
class MoveBuffer {
  public:
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    size_t freeSlots() const { return kBufferSize - count_; }
    /// Total time of the queued moves, in ms.
    uint32_t totalMs() const { return totalMs_; }

    void clear() {
        head_ = 0;
        count_ = 0;
        totalMs_ = 0;
    }

    /// Append `count` position/time pairs from `payload`, decoding each
    /// position byte within `range`. All or nothing: returns false and adds
    /// nothing if they don't all fit.
    bool pushAll(const uint8_t* payload, size_t count,
                 const Range& range = Range{}) {
        if (count > freeSlots()) {
            return false;
        }
        for (size_t i = 0; i < count; ++i) {
            moves_[(head_ + count_) % kBufferSize] = {
                decodePosition(payload[2 * i], range), payload[2 * i + 1]};
            totalMs_ += payload[2 * i + 1];
            ++count_;
        }
        return true;
    }

    /// Remove the oldest move into `out`. Returns false when empty.
    bool pop(Move& out) {
        if (count_ == 0) {
            return false;
        }
        out = moves_[head_];
        head_ = (head_ + 1) % kBufferSize;
        --count_;
        totalMs_ -= out.timeMs;
        return true;
    }

  private:
    Move moves_[kBufferSize] = {};
    size_t head_ = 0;
    size_t count_ = 0;
    uint32_t totalMs_ = 0;
};

constexpr uint16_t kDefaultLowWaterMs = 500;

/// The low-water byte of the status packet: 0 when the buffered moves (plus
/// what's left of the one running) last at least lowWaterMs, otherwise the
/// number of free slots, telling the client how many moves it may send.
inline uint8_t lowWaterSlots(uint32_t bufferedMs, uint32_t lowWaterMs,
                             size_t freeSlots) {
    if (bufferedMs >= lowWaterMs) {
        return 0;
    }
    return static_cast<uint8_t>(freeSlots);
}

/// Scale a queued position (0-65535) into a stepper target, using the same
/// stroke window as streaming_logic::scaleStreamPosition:
///   position=65535 -> -depth
///   position=0     -> -(maxStroke + depth)
inline float scaleQueuedPosition(uint16_t position, int32_t maxStroke,
                                 int32_t depth) {
    return -((1.0f - static_cast<float>(position) / 65535.0f) * maxStroke +
             depth);
}

}  // namespace queued_moves
