#include "AimFrame.h"

namespace AimFrame {

    namespace {

        size_t putBe16(uint8_t *p, uint16_t v) {
            p[0] = v >> 8;
            p[1] = v & 0xFF;
            return 2;
        }

        // Modbus sends the CRC low byte first.
        size_t appendCrc(uint8_t *frame, size_t len) {
            uint16_t crc = crc16(frame, len);
            frame[len] = crc & 0xFF;
            frame[len + 1] = crc >> 8;
            return len + 2;
        }

        bool crcOk(const uint8_t *frame, size_t len) {
            uint16_t crc = crc16(frame, len - 2);
            return frame[len - 2] == (crc & 0xFF) && frame[len - 1] == (crc >> 8);
        }

    }

    uint16_t crc16(const uint8_t *data, size_t len) {
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < len; i++) {
            crc ^= data[i];
            for (int bit = 0; bit < 8; bit++) {
                crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
            }
        }
        return crc;
    }

    size_t buildRead(uint8_t *out, uint8_t id, uint16_t addr, uint16_t count) {
        out[0] = id;
        out[1] = fcReadHolding;
        putBe16(out + 2, addr);
        putBe16(out + 4, count);
        return appendCrc(out, 6);
    }

    size_t buildWriteSingle(uint8_t *out, uint8_t id, uint16_t addr, uint16_t value) {
        out[0] = id;
        out[1] = fcWriteSingle;
        putBe16(out + 2, addr);
        putBe16(out + 4, value);
        return appendCrc(out, 6);
    }

    size_t buildWriteMultiple(uint8_t *out, uint8_t id, uint16_t addr, const uint16_t *words, uint8_t count) {
        out[0] = id;
        out[1] = fcWriteMultiple;
        putBe16(out + 2, addr);
        putBe16(out + 4, count);
        out[6] = count * 2;
        size_t len = 7;
        for (uint8_t i = 0; i < count; i++) {
            len += putBe16(out + len, words[i]);
        }
        return appendCrc(out, len);
    }

    namespace {
        // 0x78 / 0x7B request: id, fc, int32 MSB first, crc
        size_t buildInt32Frame(uint8_t *out, uint8_t id, uint8_t fc, int32_t value) {
            auto u = static_cast<uint32_t>(value);
            out[0] = id;
            out[1] = fc;
            out[2] = u >> 24;
            out[3] = (u >> 16) & 0xFF;
            out[4] = (u >> 8) & 0xFF;
            out[5] = u & 0xFF;
            return appendCrc(out, 6);
        }
    }

    size_t buildMove(uint8_t *out, uint8_t id, int32_t pulses) {
        return buildInt32Frame(out, id, fcMove, pulses);
    }

    size_t buildMoveAbsolute(uint8_t *out, uint8_t id, int32_t target) {
        return buildInt32Frame(out, id, fcMoveAbsolute, target);
    }

    size_t replyLength(uint8_t fc, uint16_t readCount) {
        switch (fc) {
            case fcReadHolding:
                return 5 + 2 * readCount;  // id, fc, byte count, data, crc
            case fcWriteSingle:
            case fcWriteMultiple:
            case fcMove:
            case fcMoveAbsolute:
                return 8;
            default:
                return 0;
        }
    }

    Reply parseReply(const uint8_t *buf, size_t len, uint8_t id, uint8_t fc, size_t expectedLen) {
        Reply r;
        while (r.offset < len && buf[r.offset] == 0x00) {
            r.offset++;
        }
        r.skipped = r.offset;

        const uint8_t *f = buf + r.offset;
        size_t avail = len - r.offset;
        if (avail < 2) {
            r.status = Status::Incomplete;
            return r;
        }
        if (f[0] != id) {
            r.status = Status::BadReply;
            return r;
        }
        if (f[1] == (fc | 0x80)) {
            if (avail < 5) {
                r.status = Status::Incomplete;
                return r;
            }
            r.exceptionCode = f[2];
            r.status = crcOk(f, 5) ? Status::Exception : Status::BadCrc;
            return r;
        }
        if (f[1] != fc) {
            r.status = Status::BadReply;
            return r;
        }
        if (avail < expectedLen) {
            r.status = Status::Incomplete;
            return r;
        }
        if (!crcOk(f, expectedLen)) {
            r.status = Status::BadCrc;
            return r;
        }
        if (fc == fcReadHolding && f[2] != expectedLen - 5) {
            r.status = Status::BadReply;
            return r;
        }
        r.status = Status::Ok;
        return r;
    }

}
