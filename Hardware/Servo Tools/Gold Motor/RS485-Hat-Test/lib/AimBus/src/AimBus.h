#pragma once

#include <Arduino.h>

#include "AimFrame.h"

/**
 * Minimal, fast Modbus RTU master for one AIM-series servo.
 *
 * Replaces eModbus on the hot path: no worker task or fixed 1.75 ms
 * inter-frame gap, and no 1 ms polling. The caller blocks until the UART's
 * RX-idle timeout fires (a couple of character times after the reply ends).
 *
 * The HardwareSerial must already be configured (pins, baud, RS485
 * half-duplex mode). Thread-safe: calls from several tasks are serialised by
 * a mutex, but one bus-owner task is the intended design.
 *
 * Retries: reads and single-register writes are idempotent and are retried.
 * writeRegisters() and move() never are, because 0x0C/0x0D and FC 0x78 are
 * *incremental* moves: if the request landed but the reply was lost, a retry
 * would move the motor twice.
 */
class AimBus {
  public:
    enum class Result : uint8_t { Ok, Timeout, BadCrc, Exception, BadReply, NotStarted };

    struct Stats {
        uint32_t transactions = 0;
        uint32_t ok = 0;
        uint32_t timeouts = 0;
        uint32_t badCrc = 0;
        uint32_t exceptions = 0;
        uint32_t badReplies = 0;
        uint32_t retries = 0;
        uint32_t skippedBytes = 0;  // leading 0x00 glitch bytes
    };

    void begin(HardwareSerial &serial, uint8_t serverId);
    void end();

    void setServerId(uint8_t id) { serverId_ = id; }
    void setTimeoutMs(uint32_t ms) { timeoutMs_ = ms; }
    void setMinGapUs(uint32_t us) { minGapUs_ = us; }  // quiet time before each request
    void setRetries(uint8_t n) { retries_ = n; }
    uint32_t minGapUs() const { return minGapUs_; }

    Result readRegisters(uint16_t addr, uint16_t count, uint16_t *out);
    Result writeRegister(uint16_t addr, uint16_t value);
    Result writeRegisters(uint16_t addr, const uint16_t *words, uint8_t count);
    Result move(int32_t pulses, int32_t *replyValue = nullptr);

    uint8_t lastExceptionCode() const { return lastException_; }
    const Stats &stats() const { return stats_; }
    void resetStats() { stats_ = Stats(); }

    static const char *resultText(Result r);

  private:
    Result transact(const uint8_t *req, size_t reqLen, uint8_t fc, size_t expectedLen, uint8_t retries,
                    const uint8_t *&frame);
    Result transactOnce(const uint8_t *req, size_t reqLen, uint8_t fc, size_t expectedLen, const uint8_t *&frame);

    HardwareSerial *serial_ = nullptr;
    SemaphoreHandle_t mutex_ = nullptr;
    volatile TaskHandle_t waiter_ = nullptr;

    uint8_t serverId_ = 1;
    uint32_t timeoutMs_ = 50;
    // The AIM57 ignores a request that starts within ~1.15 ms of its own reply (measured: every other frame
    // dropped at <=1000 us, 1/1005 at 1150 us, 0 at 1200+). 1500 us keeps ~0.3 ms of margin.
    uint32_t minGapUs_ = 1500;
    uint8_t retries_ = 1;
    uint32_t lastActivityUs_ = 0;
    uint8_t lastException_ = 0;
    Stats stats_;

    uint8_t rx_[AimFrame::maxFrame + 8] = {};
    size_t rxLen_ = 0;
};
