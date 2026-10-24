#include "AimBus.h"

namespace {
    // RX-idle timeout in character times. The UART hands bytes over (and we
    // wake) this long after the last byte: ~0.19 ms at 115200, ~1.1 ms at 19200.
    constexpr uint8_t rxIdleSymbols = 2;
}

void AimBus::begin(HardwareSerial &serial, uint8_t serverId) {
    if (!mutex_) {
        mutex_ = xSemaphoreCreateMutex();
    }
    serial_ = &serial;
    serverId_ = serverId;
    serial.setRxTimeout(rxIdleSymbols);  // must be set before onReceive(..., true)
    serial.onReceive(
        [this]() {
            TaskHandle_t waiter = waiter_;
            if (waiter) {
                xTaskNotifyGive(waiter);
            }
        },
        true);  // only on RX idle timeout, i.e. once per frame
    lastActivityUs_ = micros();
}

void AimBus::end() {
    if (serial_) {
        serial_->onReceive(nullptr);
        serial_->setRxFIFOFull(1);  // undo onReceive's 120-byte FIFO threshold for other users
        serial_ = nullptr;
    }
}

const char *AimBus::resultText(Result r) {
    switch (r) {
        case Result::Ok:
            return "ok";
        case Result::Timeout:
            return "timeout";
        case Result::BadCrc:
            return "bad CRC";
        case Result::Exception:
            return "Modbus exception";
        case Result::BadReply:
            return "bad reply";
        case Result::NotStarted:
            return "bus not started";
    }
    return "?";
}

AimBus::Result AimBus::readRegisters(uint16_t addr, uint16_t count, uint16_t *out) {
    if (count == 0 || count > AimFrame::maxReadCount) {
        return Result::BadReply;
    }
    uint8_t req[8];
    size_t len = AimFrame::buildRead(req, serverId_, addr, count);
    const uint8_t *frame = nullptr;
    Result r = transact(req, len, AimFrame::fcReadHolding, AimFrame::replyLength(AimFrame::fcReadHolding, count),
                        retries_, frame);
    if (r == Result::Ok) {
        for (uint16_t i = 0; i < count; i++) {
            out[i] = AimFrame::be16(frame + 3 + 2 * i);
        }
    }
    return r;
}

AimBus::Result AimBus::writeRegister(uint16_t addr, uint16_t value) {
    uint8_t req[8];
    size_t len = AimFrame::buildWriteSingle(req, serverId_, addr, value);
    const uint8_t *frame = nullptr;
    Result r = transact(req, len, AimFrame::fcWriteSingle, AimFrame::replyLength(AimFrame::fcWriteSingle), retries_,
                        frame);
    if (r == Result::Ok && (AimFrame::be16(frame + 2) != addr || AimFrame::be16(frame + 4) != value)) {
        r = Result::BadReply;  // echo must match the request
    }
    return r;
}

AimBus::Result AimBus::writeRegisters(uint16_t addr, const uint16_t *words, uint8_t count) {
    if (count == 0 || count > 16) {
        return Result::BadReply;
    }
    uint8_t req[9 + 2 * 16];
    size_t len = AimFrame::buildWriteMultiple(req, serverId_, addr, words, count);
    const uint8_t *frame = nullptr;
    return transact(req, len, AimFrame::fcWriteMultiple, AimFrame::replyLength(AimFrame::fcWriteMultiple), 0, frame);
}

AimBus::Result AimBus::move(int32_t pulses, int32_t *replyValue) {
    uint8_t req[8];
    size_t len = AimFrame::buildMove(req, serverId_, pulses);
    const uint8_t *frame = nullptr;
    Result r = transact(req, len, AimFrame::fcMove, AimFrame::replyLength(AimFrame::fcMove), 0, frame);
    if (r == Result::Ok && replyValue) {
        *replyValue = AimFrame::moveReplyValue(frame);
    }
    return r;
}

AimBus::Result AimBus::transact(const uint8_t *req, size_t reqLen, uint8_t fc, size_t expectedLen, uint8_t retries,
                                const uint8_t *&frame) {
    if (!serial_) {
        return Result::NotStarted;
    }
    xSemaphoreTake(mutex_, portMAX_DELAY);
    Result r = transactOnce(req, reqLen, fc, expectedLen, frame);
    // An exception is a valid answer, so only retry when we didn't get one.
    for (uint8_t i = 0; i < retries && r != Result::Ok && r != Result::Exception; i++) {
        stats_.retries++;
        r = transactOnce(req, reqLen, fc, expectedLen, frame);
    }
    xSemaphoreGive(mutex_);
    return r;
}

AimBus::Result AimBus::transactOnce(const uint8_t *req, size_t reqLen, uint8_t fc, size_t expectedLen,
                                    const uint8_t *&frame) {
    stats_.transactions++;

    while (micros() - lastActivityUs_ < minGapUs_) {
        // short busy-wait: minGapUs_ is at most a couple of ms
    }

    // Drop anything stale (late replies, noise) and any leftover wake-up.
    while (serial_->available()) {
        serial_->read();
    }
    waiter_ = xTaskGetCurrentTaskHandle();
    ulTaskNotifyTake(pdTRUE, 0);

    serial_->write(req, reqLen);
    serial_->flush();  // returns once the last bit is out; the UART then drops DE

    rxLen_ = 0;
    Result result = Result::Timeout;
    const uint32_t start = millis();
    for (;;) {
        while (serial_->available() && rxLen_ < sizeof(rx_)) {
            rx_[rxLen_++] = serial_->read();
        }
        AimFrame::Reply reply = AimFrame::parseReply(rx_, rxLen_, serverId_, fc, expectedLen);
        if (reply.status != AimFrame::Status::Incomplete) {
            stats_.skippedBytes += reply.skipped;
            frame = rx_ + reply.offset;
            switch (reply.status) {
                case AimFrame::Status::Ok:
                    result = Result::Ok;
                    break;
                case AimFrame::Status::Exception:
                    lastException_ = reply.exceptionCode;
                    result = Result::Exception;
                    break;
                case AimFrame::Status::BadCrc:
                    result = Result::BadCrc;
                    break;
                default:
                    result = Result::BadReply;
                    break;
            }
            break;
        }
        uint32_t elapsed = millis() - start;
        if (elapsed >= timeoutMs_ || rxLen_ >= sizeof(rx_)) {
            break;
        }
        // Woken by the RX-idle callback; the timeout is only a safety net.
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(timeoutMs_ - elapsed) + 1);
    }

    waiter_ = nullptr;
    lastActivityUs_ = micros();
    switch (result) {
        case Result::Ok:
            stats_.ok++;
            break;
        case Result::Timeout:
            stats_.timeouts++;
            break;
        case Result::BadCrc:
            stats_.badCrc++;
            break;
        case Result::Exception:
            stats_.exceptions++;
            break;
        default:
            stats_.badReplies++;
            break;
    }
    return result;
}
