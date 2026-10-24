#pragma once

#include <cstddef>
#include <cstdint>

/**
 * Modbus RTU framing for the AIM-series servo (57AIM30 "Gold Motor").
 *
 * Pure C++ (no Arduino/ESP-IDF) so it can be unit-tested anywhere. Covers the
 * four function codes the OSSM needs: 0x03 read, 0x06 write one, 0x10 write
 * many, and the vendor 0x78 incremental move.
 */
namespace AimFrame {

    constexpr uint8_t fcReadHolding = 0x03;
    constexpr uint8_t fcWriteSingle = 0x06;
    constexpr uint8_t fcWriteMultiple = 0x10;
    constexpr uint8_t fcMove = 0x78;          // vendor: incremental move, 4-byte signed pulse count
    constexpr uint8_t fcMoveAbsolute = 0x7B;  // vendor: absolute move, 4-byte signed target

    constexpr size_t maxReadCount = 32;  // plenty for the 26-register map
    constexpr size_t maxFrame = 5 + 2 * maxReadCount;

    uint16_t crc16(const uint8_t *data, size_t len);

    // Builders write a complete frame (with CRC) into out and return its length.
    size_t buildRead(uint8_t *out, uint8_t id, uint16_t addr, uint16_t count);
    size_t buildWriteSingle(uint8_t *out, uint8_t id, uint16_t addr, uint16_t value);
    size_t buildWriteMultiple(uint8_t *out, uint8_t id, uint16_t addr, const uint16_t *words, uint8_t count);
    size_t buildMove(uint8_t *out, uint8_t id, int32_t pulses);
    size_t buildMoveAbsolute(uint8_t *out, uint8_t id, int32_t target);

    // Length of a normal (non-exception) reply, including CRC.
    size_t replyLength(uint8_t fc, uint16_t readCount = 0);

    enum class Status : uint8_t {
        Ok,
        Incomplete,  // keep receiving
        BadCrc,
        Exception,   // motor answered with fc | 0x80
        BadReply,    // wrong ID / FC / byte count
    };

    struct Reply {
        Status status = Status::Incomplete;
        size_t offset = 0;         // start of the frame inside the buffer
        size_t skipped = 0;        // leading 0x00 glitch bytes ignored
        uint8_t exceptionCode = 0;
    };

    /**
     * Find and validate the reply in buf. Leading 0x00 bytes are skipped: the
     * V2.3 hat sees one in front of every reply (line glitch as DE drops), and
     * 0 is never a valid server ID.
     */
    Reply parseReply(const uint8_t *buf, size_t len, uint8_t id, uint8_t fc, size_t expectedLen);

    inline uint16_t be16(const uint8_t *p) { return static_cast<uint16_t>((p[0] << 8) | p[1]); }

    // 0x78/0x7B replies carry an int32 as two big-endian words, low word first (e.g. FF F9 FF FF = -7).
    inline int32_t moveReplyValue(const uint8_t *frame) {
        return static_cast<int32_t>((static_cast<uint32_t>(be16(frame + 4)) << 16) | be16(frame + 2));
    }

}
