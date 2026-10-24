/**
 * RS485 hat bring-up + AIM57 (Gold Motor) parameter tool.
 *
 * Talks Modbus RTU to the motor through the ESP32's UART in hardware RS485
 * half-duplex mode (RTS drives the transceiver's DE/~RE). Pins come from
 * platformio.ini so the same code runs on the V2.3 hat and on V2.5.
 *
 * Serial console at 115200, type `help`.
 */
#include <Arduino.h>

#include "AimBus.h"
#include "AimRegisters.h"
#include "ModbusClientRTU.h"
#include "RTUutils.h"

#ifndef RS485_TX_PIN
#error "Define RS485_TX_PIN / RS485_RX_PIN / RS485_DE_PIN in platformio.ini"
#endif

namespace {

    constexpr uint32_t requestTimeoutMs = 200;
    constexpr uint32_t confirmTimeoutMs = 15000;

    ModbusClientRTU MB(-1);  // -1: DE is driven by the UART (RS485 mode), not by eModbus

    uint8_t serverId = Aim::defaultServerId;
    uint32_t baud = Aim::defaultBaud;
    uint32_t nextToken = 1;
    // Every AIM57 reply on the V2.3 hat starts with a stray 0x00 (line glitch as DE drops), so skip it by default.
    bool skipLeadingZero = true;

    // Two interchangeable backends on the same UART: eModbus (library) and AimBus (our raw RTU master).
    AimBus aimBus;
    bool useRawBus = false;

    // ---------------------------------------------------------------- transport

    void startBus(uint32_t newBaud) {
        baud = newBaud;
        Serial2.end();
        RTUutils::prepareHardwareSerial(Serial2);
        Serial2.begin(baud, SERIAL_8N1, RS485_RX_PIN, RS485_TX_PIN);
        // RTS -> DE/~RE; the UART raises it only while transmitting.
        bool pinsOk = Serial2.setPins(RS485_RX_PIN, RS485_TX_PIN, -1, RS485_DE_PIN);
        bool modeOk = Serial2.setMode(UART_MODE_RS485_HALF_DUPLEX);
        if (!pinsOk || !modeOk) {
            Serial.printf("!! UART RS485 setup failed (setPins=%d setMode=%d)\n", pinsOk, modeOk);
        }
        if (useRawBus) {
            MB.end();
            aimBus.begin(Serial2, serverId);
            // A reply is back in ~2 ms at 115200; a short timeout keeps a rare miss cheap.
            aimBus.setTimeoutMs(baud >= 57600 ? 10 : 50);
        } else {
            aimBus.end();
            MB.begin(Serial2);  // restarts the worker and recomputes the frame gap for this baud
            MB.setTimeout(requestTimeoutMs);
            MB.skipLeading0x00(skipLeadingZero);
        }
    }

    Error toError(AimBus::Result r) {
        switch (r) {
            case AimBus::Result::Ok:
                return SUCCESS;
            case AimBus::Result::Timeout:
                return TIMEOUT;
            case AimBus::Result::BadCrc:
                return CRC_ERROR;
            case AimBus::Result::Exception:
                return static_cast<Error>(aimBus.lastExceptionCode());
            case AimBus::Result::BadReply:
                return PACKET_LENGTH_ERROR;
            default:
                return UNDEFINED_ERROR;
        }
    }

    // eModbus' syncRequest() polls for the reply every 10 ms, which caps a request at ~20 ms
    // (~50 Hz). Instead we queue with addRequest() and the response handler wakes us directly.
    bool notifyWait = true;
    TaskHandle_t waitingTask = nullptr;
    volatile uint32_t pendingToken = 0;
    uint8_t pendingFc = 0;
    ModbusMessage pendingResponse;

    void onResponse(ModbusMessage msg, uint32_t token) {
        if (token == pendingToken && waitingTask) {
            pendingResponse = msg;
            xTaskNotifyGive(waitingTask);
        }
    }

    void onRequestError(Error err, uint32_t token) {
        if (token == pendingToken && waitingTask) {
            pendingResponse.setError(serverId, pendingFc, err);
            xTaskNotifyGive(waitingTask);
        }
    }

    template <typename... Args>
    ModbusMessage request(uint8_t fc, Args &&...args) {
        uint32_t token = nextToken++;
        if (!notifyWait) {
            return MB.syncRequest(token, serverId, fc, std::forward<Args>(args)...);
        }
        ModbusMessage failure;
        waitingTask = xTaskGetCurrentTaskHandle();
        pendingFc = fc;
        pendingToken = token;
        ulTaskNotifyTake(pdTRUE, 0);  // clear any stale notification
        Error err = MB.addRequest(token, serverId, fc, std::forward<Args>(args)...);
        if (err != SUCCESS) {
            failure.setError(serverId, fc, err);
            return failure;
        }
        if (!ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(requestTimeoutMs + 100))) {
            failure.setError(serverId, fc, TIMEOUT);
            return failure;
        }
        return pendingResponse;
    }

    const char *errorText(Error e) {
        ModbusError me(e);
        return static_cast<const char *>(me);
    }

    void printHint(Error e) {
        switch (e) {
            case TIMEOUT:
                Serial.println("   hint: no reply. Check motor power (green LED), +5V on hat J5-1, "
                               "A/B wiring, ID/baud (`scan`), and that DE (GPIO" +
                               String(RS485_DE_PIN) + ") pulses high during TX.");
                break;
            case CRC_ERROR:
            case PACKET_LENGTH_ERROR:
                Serial.println("   hint: garbled reply. Check A/B not swapped, common GND, baud.");
                break;
            case ILLEGAL_DATA_ADDRESS:
            case ILLEGAL_DATA_VALUE:
            case ILLEGAL_FUNCTION:
                Serial.println("   hint: motor answered but rejected the request (comms are OK).");
                break;
            default:
                break;
        }
    }

    Error readRegs(uint16_t addr, uint16_t count, uint16_t *out) {
        if (useRawBus) {
            return toError(aimBus.readRegisters(addr, count, out));
        }
        ModbusMessage rsp = request(READ_HOLD_REGISTER, addr, count);
        Error err = rsp.getError();
        if (err != SUCCESS) {
            return err;
        }
        if (rsp.size() < 3 + 2 * count || rsp[2] != 2 * count) {
            return PACKET_LENGTH_ERROR;
        }
        uint16_t idx = 3;
        for (uint16_t i = 0; i < count; i++) {
            idx = rsp.get(idx, out[i]);
        }
        return SUCCESS;
    }

    // Block read, falling back to one-at-a-time if the motor refuses multi-register reads.
    Error readRange(uint16_t addr, uint16_t count, uint16_t *out) {
        Error err = readRegs(addr, count, out);
        if (err == SUCCESS || err == TIMEOUT || count == 1) {
            return err;
        }
        for (uint16_t i = 0; i < count; i++) {
            err = readRegs(addr + i, 1, &out[i]);
            if (err != SUCCESS) {
                return err;
            }
        }
        return SUCCESS;
    }

    Error writeReg(uint16_t addr, uint16_t value) {
        if (useRawBus) {
            return toError(aimBus.writeRegister(addr, value));  // AimBus checks the echo itself
        }
        ModbusMessage rsp = request(WRITE_HOLD_REGISTER, addr, value);
        Error err = rsp.getError();
        if (err != SUCCESS) {
            return err;
        }
        uint16_t echoAddr = 0, echoValue = 0;
        rsp.get(2, echoAddr, echoValue);
        if (echoAddr != addr || echoValue != value) {
            return UNDEFINED_ERROR;
        }
        return SUCCESS;
    }

    bool checkedWrite(uint16_t addr, uint16_t value) {
        Error err = writeReg(addr, value);
        if (err != SUCCESS) {
            Serial.printf("   write 0x%02X = %u FAILED: %s\n", addr, value, errorText(err));
            printHint(err);
            return false;
        }
        Serial.printf("   write 0x%02X = %u ok\n", addr, value);
        return true;
    }

    bool checkedRead(uint16_t addr, uint16_t &value) {
        Error err = readRegs(addr, 1, &value);
        if (err != SUCCESS) {
            Serial.printf("   read 0x%02X FAILED: %s\n", addr, errorText(err));
            printHint(err);
            return false;
        }
        return true;
    }

    // Parameter writes only work with 0x00 = 1. Runs fn with it enabled, then puts 0x00 back.
    template <typename F>
    bool withModbusEnabled(F fn) {
        uint16_t previous = 0;
        if (!checkedRead(Aim::Reg::modbusEnable, previous)) {
            return false;
        }
        if (previous != 1 && !checkedWrite(Aim::Reg::modbusEnable, 1)) {
            return false;
        }
        bool ok = fn();
        if (previous != 1) {
            ok = checkedWrite(Aim::Reg::modbusEnable, previous) && ok;
        }
        return ok;
    }

    // ---------------------------------------------------------------- console input

    String readLineBlocking(uint32_t timeoutMs) {
        String line;
        uint32_t start = millis();
        while (millis() - start < timeoutMs) {
            while (Serial.available()) {
                char c = Serial.read();
                if (c == '\r' || c == '\n') {
                    if (line.length()) {
                        Serial.println();
                        return line;
                    }
                    continue;
                }
                line += c;
                Serial.print(c);
            }
            delay(5);
        }
        Serial.println();
        return line;
    }

    bool confirm(const char *question) {
        Serial.printf("%s [y/N] ", question);
        String answer = readLineBlocking(confirmTimeoutMs);
        answer.trim();
        return answer.equalsIgnoreCase("y");
    }

    bool parseNumber(const String &s, long &out) {
        if (s.isEmpty()) {
            return false;
        }
        char *end = nullptr;
        out = strtol(s.c_str(), &end, 0);  // accepts 0x.. hex
        return end && *end == '\0';
    }

    // ---------------------------------------------------------------- formatting

    void printValue(const Aim::Register &r, uint16_t raw) {
        Serial.printf("0x%02X %-22s %6u (0x%04X)", r.addr, r.name, raw, raw);
        if (r.scale != 0) {
            float v = r.isSigned ? static_cast<int16_t>(raw) * r.scale : raw * r.scale;
            Serial.printf("  = %.3f %s", v, r.unit);
        }
        if (r.addr == Aim::Reg::alarmCode) {
            Serial.printf("  (%s)", Aim::alarmText(raw));
        }
        Serial.printf("  %s", r.writable ? "RW" : "RO");
        if (r.note[0]) {
            Serial.printf("  ; %s", r.note);
        }
        Serial.println();
    }

    int32_t combine32(uint16_t low, uint16_t high) {
        return static_cast<int32_t>((static_cast<uint32_t>(high) << 16) | low);
    }

    // ---------------------------------------------------------------- commands

    bool cmdPing() {
        Serial.printf("ping: ID %u @ %lu baud (TX=%d RX=%d DE=%d)\n", serverId, baud, RS485_TX_PIN,
                      RS485_RX_PIN, RS485_DE_PIN);
        uint16_t steps = 0, alarm = 0, volts = 0, temp = 0;
        uint32_t t0 = millis();
        Error err = readRegs(Aim::Reg::stepsPerRev, 1, &steps);
        uint32_t rtt = millis() - t0;
        if (err != SUCCESS) {
            Serial.printf("   FAIL: %s\n", errorText(err));
            printHint(err);
            return false;
        }
        readRegs(Aim::Reg::alarmCode, 1, &alarm);
        readRegs(Aim::Reg::voltage, 1, &volts);
        readRegs(Aim::Reg::temperature, 1, &temp);
        Serial.printf("   OK in %lu ms: steps/rev %u, %.1f V, %u C, alarm 0x%02X (%s)\n", rtt, steps,
                      volts / 327.0f, temp, alarm, Aim::alarmText(alarm));
        return true;
    }

    // Bypasses eModbus: sends one FC03 read of 0x0B and hex-dumps every byte that comes back.
    void cmdSniff() {
        uint8_t req[8] = {serverId, READ_HOLD_REGISTER, 0x00, Aim::Reg::stepsPerRev, 0x00, 0x01};
        uint16_t crc = RTUutils::calcCRC(req, 6);
        req[6] = crc & 0xFF;  // Modbus sends CRC low byte first
        req[7] = crc >> 8;

        MB.end();  // stop the eModbus worker so it doesn't consume the reply
        while (Serial2.available()) {
            Serial2.read();
        }
        Serial.print("   TX:");
        for (uint8_t b : req) {
            Serial.printf(" %02X", b);
        }
        Serial.println();

        Serial2.write(req, sizeof(req));
        Serial2.flush();
        uint8_t rx[64];
        size_t n = 0;
        uint32_t start = millis(), last = start;
        while (millis() - start < 300 && n < sizeof(rx)) {
            if (Serial2.available()) {
                rx[n++] = Serial2.read();
                last = millis();
            } else if (n && millis() - last > 20) {
                break;
            }
        }
        Serial.printf("   RX (%u bytes, first after ~%lu ms):", n, n ? last - start : 0);
        for (size_t i = 0; i < n; i++) {
            Serial.printf(" %02X", rx[i]);
        }
        Serial.println();
        // Expected good reply: 01 03 02 <hi> <lo> <crc lo> <crc hi>
        for (size_t off = 0; off + 7 <= n; off++) {
            if (rx[off] == serverId && rx[off + 1] == READ_HOLD_REGISTER && rx[off + 2] == 2) {
                uint16_t c = RTUutils::calcCRC(&rx[off], 5);
                bool ok = rx[off + 5] == (c & 0xFF) && rx[off + 6] == (c >> 8);
                Serial.printf("   frame at offset %u: value %u, CRC %s\n", off, (rx[off + 3] << 8) | rx[off + 4],
                              ok ? "OK" : "BAD");
            }
        }
        startBus(baud);
    }

    void cmdScan() {
        static const uint32_t bauds[] = {19200, 9600, 38400, 115200};
        uint32_t oldBaud = baud;
        uint8_t oldId = serverId;
        bool found = false;
        MB.setTimeout(100);
        for (uint32_t b : bauds) {
            startBus(b);
            MB.setTimeout(100);
            for (uint8_t id = 1; id <= 5; id++) {
                serverId = id;
                uint16_t v = 0;
                Error err = readRegs(Aim::Reg::stepsPerRev, 1, &v);
                Serial.printf("   %6lu baud, ID %u: %s\n", b, id,
                              err == SUCCESS ? "ANSWERED" : errorText(err));
                if (err == SUCCESS && !found) {
                    found = true;
                    oldBaud = b;
                    oldId = id;
                }
            }
        }
        serverId = oldId;
        startBus(oldBaud);
        Serial.printf("scan done; using ID %u @ %lu baud%s\n", serverId, baud,
                      found ? "" : " (nothing answered)");
    }

    void cmdDump() {
        uint16_t values[Aim::registerCount] = {};
        Error err = readRange(0, Aim::registerCount, values);
        if (err != SUCCESS) {
            Serial.printf("dump FAILED: %s\n", errorText(err));
            printHint(err);
            return;
        }
        for (const auto &r : Aim::registers) {
            printValue(r, values[r.addr]);
        }
        Serial.printf("     ABS_POSITION (int32)        %ld\n",
                      static_cast<long>(combine32(values[Aim::Reg::absPositionLow],
                                                  values[Aim::Reg::absPositionHigh])));
    }

    void cmdRead(uint16_t addr) {
        uint16_t v = 0;
        if (!checkedRead(addr, v)) {
            return;
        }
        if (const auto *r = Aim::find(addr)) {
            printValue(*r, v);
        } else {
            Serial.printf("0x%02X = %u (0x%04X) (not in register table)\n", addr, v, v);
        }
    }

    void cmdWrite(uint16_t addr, long value) {
        const auto *r = Aim::find(addr);
        if (!r) {
            Serial.println("unknown register (0x00-0x19)");
            return;
        }
        if (!r->writable) {
            Serial.printf("%s is read-only over FC 0x06\n", r->name);
            return;
        }
        if (value < 0 || value > 0xFFFF) {
            Serial.println("value must be 0-65535");
            return;
        }
        auto v = static_cast<uint16_t>(value);
        if (v < r->min || v > r->max) {
            Serial.printf("%u is outside the manual's range %u-%u for %s.\n", v, r->min, r->max, r->name);
            if (!confirm("Write anyway?")) {
                return;
            }
        }
        if (Aim::needsConfirmation(*r, v)) {
            Serial.printf("%s: %s.\n", r->name, r->note);
            if (!confirm("This can change how the motor behaves. Continue?")) {
                return;
            }
        }

        auto doWrite = [&]() {
            if (!checkedWrite(addr, v)) {
                return false;
            }
            uint16_t back = 0;
            if (!checkedRead(addr, back)) {
                return false;
            }
            Serial.printf("   read-back: %u %s\n", back, back == v ? "(match)" : "(MISMATCH)");
            return back == v;
        };
        // 0x00 is the enable itself, so it doesn't need the wrapper.
        bool ok = addr == Aim::Reg::modbusEnable ? doWrite() : withModbusEnabled(doWrite);
        Serial.println(ok ? "write OK (in RAM - use `save` to persist)" : "write FAILED");
    }

    void cmdSave() {
        bool ok = withModbusEnabled([] { return checkedWrite(Aim::Reg::saveFlag, 1); });
        delay(200);
        Serial.println(ok ? "save OK" : "save FAILED");
    }

    void cmdMonitor() {
        Serial.println("monitor: press any key to stop");
        while (Serial.available()) {
            Serial.read();
        }
        while (!Serial.available()) {
            uint16_t live[6] = {};  // 0x0E..0x13
            uint16_t pos[2] = {};
            Error err = readRange(Aim::Reg::alarmCode, 6, live);
            if (err == SUCCESS) {
                err = readRange(Aim::Reg::absPositionLow, 2, pos);
            }
            if (err != SUCCESS) {
                Serial.printf("   read FAILED: %s\n", errorText(err));
            } else {
                Serial.printf("%7.2f V  %6.3f A  %7.1f rpm  %3u C  pwm %6.1f%%  pos %8ld  alarm 0x%02X\n",
                              live[3] / 327.0f, live[1] / 2000.0f, static_cast<int16_t>(live[2]) / 10.0f,
                              live[4], static_cast<int16_t>(live[5]) * 100.0f / 32768,
                              static_cast<long>(combine32(pos[0], pos[1])), live[0]);
            }
            delay(500);
        }
        while (Serial.available()) {
            Serial.read();
        }
    }

    // ---------------------------------------------------------------- benchmark

    void printBusStats() {
        const auto &s = aimBus.stats();
        Serial.printf("   AimBus: %u tx, %u ok, %u timeout, %u bad CRC, %u exception, %u bad reply, %u retries, "
                      "%u glitch bytes skipped\n",
                      s.transactions, s.ok, s.timeouts, s.badCrc, s.exceptions, s.badReplies, s.retries,
                      s.skippedBytes);
    }

    struct BenchStats {
        uint32_t ok = 0;
        uint32_t fail = 0;
        uint32_t minUs = UINT32_MAX;
        uint32_t maxUs = 0;
        uint64_t totalUs = 0;
        Error lastError = SUCCESS;

        void add(uint32_t us, Error err) {
            if (err != SUCCESS) {
                fail++;
                lastError = err;
                return;
            }
            ok++;
            totalUs += us;
            minUs = min(minUs, us);
            maxUs = max(maxUs, us);
        }

        void print(const char *label, uint16_t wireBytes) const {
            if (!ok) {
                Serial.printf("   %-6s no successful transactions (%u failed, last: %s)\n", label, fail,
                              errorText(lastError));
                return;
            }
            float avgMs = totalUs / 1000.0f / ok;
            float wireMs = wireBytes * 10 * 1000.0f / baud;  // 8N1 = 10 bits per byte
            Serial.printf("   %-6s %4u ok %3u fail | min %6.2f  avg %6.2f  max %6.2f ms | %6.1f Hz"
                          " | wire floor %5.2f ms (%u B)\n",
                          label, ok, fail, minUs / 1000.0f, avgMs, maxUs / 1000.0f, 1000.0f / avgMs, wireMs,
                          wireBytes);
        }
    };

    // Frame sizes incl. CRC, for the wire-time floor
    constexpr uint16_t positionReadBytes = 8 + 9;    // FC03 x2 request + response
    constexpr uint16_t positionWriteBytes = 13 + 8;  // FC10 x2 request + response
    constexpr uint16_t move78Bytes = 8 + 8;          // FC 0x78 request + response
    constexpr uint8_t fcIncrementalMove = 0x78;      // AIM vendor FC: 4-byte signed pulse count, MSB first
    constexpr int32_t maxBenchDrift = 20;            // abort if the motor moves more than this

    Error readPosition(int32_t &pos) {
        uint16_t words[2] = {};
        Error err = readRegs(Aim::Reg::absPositionLow, 2, words);
        if (err == SUCCESS) {
            pos = combine32(words[0], words[1]);
        }
        return err;
    }

    // Incremental move of 0 steps: the same frame as a real position command, but the motor stays put.
    Error writeZeroMove() {
        uint16_t words[2] = {0, 0};
        if (useRawBus) {
            return toError(aimBus.writeRegisters(Aim::Reg::targetPositionLow, words, 2));
        }
        ModbusMessage rsp = request(WRITE_MULT_REGISTERS, Aim::Reg::targetPositionLow, static_cast<uint16_t>(2),
                                    static_cast<uint8_t>(4), words);
        return rsp.getError();
    }

    // Same 0-step move through the vendor FC 0x78: 8-byte frames instead of 13.
    Error writeZeroMove78(bool printReply = false) {
        if (useRawBus) {
            int32_t value = 0;
            AimBus::Result r = aimBus.move(0, &value);
            if (printReply) {
                Serial.printf("   FC78 reply value: %ld (%s)\n", static_cast<long>(value), AimBus::resultText(r));
            }
            return toError(r);
        }
        uint8_t pulses[4] = {0, 0, 0, 0};
        ModbusMessage rsp = request(fcIncrementalMove, static_cast<uint16_t>(4), pulses);
        if (printReply) {
            Serial.print("   FC78 reply:");
            for (uint16_t i = 0; i < rsp.size(); i++) {
                Serial.printf(" %02X", rsp[i]);
            }
            Serial.println();
        }
        return rsp.getError();
    }

    void cmdBench(String mode, long iterations) {
        mode.toLowerCase();
        bool use78 = mode.endsWith("78");
        if (use78) {
            mode.remove(mode.length() - 2);
        }
        bool doWrite = mode == "write" || mode == "both";
        bool doRead = mode == "read" || mode == "both";
        if (!doWrite && !doRead) {
            Serial.println("usage: bench read|write|both|write78|both78 [iterations]");
            return;
        }
        auto writeMove = [use78]() { return use78 ? writeZeroMove78() : writeZeroMove(); };
        const uint16_t writeBytes = use78 ? move78Bytes : positionWriteBytes;
        if (use78) {
            mode += "78";
        }
        uint32_t n = constrain(iterations, 1, 200000);

        int32_t startPos = 0;
        if (Error err = readPosition(startPos); err != SUCCESS) {
            Serial.printf("bench aborted: can't read position (%s)\n", errorText(err));
            printHint(err);
            return;
        }

        uint16_t previousEnable = 0;
        if (doWrite) {
            // Position commands need Modbus control (0x00 = 1), which also ignores STEP/DIR until restored.
            if (!checkedRead(Aim::Reg::modbusEnable, previousEnable) ||
                (previousEnable != 1 && !checkedWrite(Aim::Reg::modbusEnable, 1))) {
                Serial.println("bench aborted: couldn't enable Modbus control");
                return;
            }
        }

        Serial.printf("bench %s x%u @ %lu baud on %s (gap %lu us), start position %ld (any key aborts)\n",
                      mode.c_str(), n, baud, useRawBus ? "AimBus" : "eModbus", useRawBus ? aimBus.minGapUs() : 0,
                      static_cast<long>(startPos));
        aimBus.resetStats();
        if (use78) {
            writeZeroMove78(true);  // show what the motor sends back once
        }
        while (Serial.available()) {
            Serial.read();
        }

        BenchStats writes, reads, cycles;
        const char *abortReason = nullptr;
        uint32_t consecutiveFails = 0;
        int32_t worstDrift = 0;
        uint32_t benchStart = millis();

        for (uint32_t i = 0; i < n && !abortReason; i++) {
            if (Serial.available()) {
                abortReason = "key pressed";
                break;
            }
            uint32_t cycleStart = micros();
            Error cycleErr = SUCCESS;

            if (doWrite) {
                uint32_t t = micros();
                Error err = writeMove();
                writes.add(micros() - t, err);
                cycleErr = err;
            }
            if (doRead) {
                int32_t pos = 0;
                uint32_t t = micros();
                Error err = readPosition(pos);
                reads.add(micros() - t, err);
                if (err == SUCCESS) {
                    int32_t drift = abs(pos - startPos);
                    worstDrift = max(worstDrift, drift);
                    if (drift > maxBenchDrift) {
                        abortReason = "position moved - stopping";
                    }
                } else if (cycleErr == SUCCESS) {
                    cycleErr = err;
                }
            }
            cycles.add(micros() - cycleStart, cycleErr);

            consecutiveFails = cycleErr == SUCCESS ? 0 : consecutiveFails + 1;
            if (consecutiveFails >= 10) {
                abortReason = "10 failures in a row";
            }
        }
        uint32_t elapsedMs = millis() - benchStart;

        if (doWrite) {
            writeMove();  // a 0-step move doubles as the manual's "stop" command
            if (previousEnable != 1) {
                checkedWrite(Aim::Reg::modbusEnable, previousEnable);
            }
        }
        int32_t endPos = startPos;
        readPosition(endPos);

        if (abortReason) {
            Serial.printf("   ABORTED: %s\n", abortReason);
        }
        if (doWrite) {
            writes.print(use78 ? "wr78" : "write", writeBytes);
        }
        if (doRead) {
            reads.print("read", positionReadBytes);
        }
        if (doWrite && doRead) {
            cycles.print("cycle", writeBytes + positionReadBytes);
        }
        Serial.printf("   %u cycles in %lu ms; position %ld -> %ld (worst drift %ld)\n", cycles.ok + cycles.fail,
                      elapsedMs, static_cast<long>(startPos), static_cast<long>(endPos),
                      static_cast<long>(worstDrift));
        if (useRawBus) {
            printBusStats();
        }
    }

    // Manual §5.6 "modify the baud rate": 0x00=1, accel=code, weak-magnetic=129, 0x00=506, then power-cycle.
    // No save needed. The manual's step list says accel/weak-magnetic are at "address 2/3", but its register
    // table puts them at 0x03/0x04, so variant 'a' follows the names and 'b' the literal addresses.
    void cmdSetBaud(long newBaud, String variant) {
        uint16_t code;
        switch (newBaud) {
            case 9600:
                code = 800;
                break;
            case 19200:
                code = 801;
                break;
            case 38400:
                code = 802;
                break;
            case 115200:
                code = 803;
                break;
            default:
                Serial.println("baud must be 9600, 19200, 38400 or 115200");
                return;
        }
        variant.toLowerCase();
        if (variant != "a" && variant != "b") {
            Serial.println("usage: setbaud <baud> a|b   (a = regs 0x03/0x04 per names, b = 0x02/0x03 literal)");
            return;
        }
        const uint16_t codeReg = variant == "a" ? 0x03 : 0x02;
        const uint16_t magicReg = codeReg + 1;

        uint16_t before[3] = {};
        if (readRange(0x02, 3, before) != SUCCESS) {
            Serial.println("FAIL: couldn't read 0x02-0x04");
            return;
        }
        Serial.printf("   before: 0x02=%u 0x03=%u 0x04=%u (RAM only; reloaded at power-up)\n", before[0], before[1],
                      before[2]);
        Serial.printf("   variant %s: 0x00=1, 0x%02X=%u, 0x%02X=129, 0x00=506\n", variant.c_str(), codeReg, code,
                      magicReg);

        const uint16_t steps[][2] = {{Aim::Reg::modbusEnable, 1}, {codeReg, code}, {magicReg, 129},
                                     {Aim::Reg::modbusEnable, 506}};
        for (const auto &step : steps) {
            Error err = writeReg(step[0], step[1]);
            Serial.printf("   write 0x%02X = %u: %s\n", step[0], step[1], err == SUCCESS ? "ok" : errorText(err));
        }
        Serial.printf("Now power-cycle the motor, then `scan` (or `baud %ld` + `ping`).\n", newBaud);
    }

    void cmdSteps(long target) {
        if (target < 1 || target > 0xFFFF) {
            Serial.println("steps must be 1-65535");
            return;
        }
        auto steps = static_cast<uint16_t>(target);
        Serial.printf("== set steps/rev to %u ==\n", steps);

        uint16_t origEnable = 0, origOutput = 0, numerator = 0, before = 0;
        if (!checkedRead(Aim::Reg::modbusEnable, origEnable) ||
            !checkedRead(Aim::Reg::driveOutputEnable, origOutput) ||
            !checkedRead(Aim::Reg::gearNumerator, numerator) ||
            !checkedRead(Aim::Reg::stepsPerRev, before)) {
            Serial.println("FAIL: could not read current state");
            return;
        }
        Serial.printf("   before: 0x00=%u 0x01=%u numerator=%u steps/rev=%u\n", origEnable, origOutput,
                      numerator, before);
        if (numerator == 0) {
            Serial.println("FAIL: gear numerator is 0 (special mode) - fix that first");
            return;
        }

        Serial.println("   motor output goes off briefly - keep the rail unloaded");
        bool ok = checkedWrite(Aim::Reg::modbusEnable, 1) && checkedWrite(Aim::Reg::driveOutputEnable, 0) &&
                  checkedWrite(Aim::Reg::stepsPerRev, steps);
        uint16_t ram = 0;
        if (ok) {
            ok = checkedRead(Aim::Reg::stepsPerRev, ram) && ram == steps;
            Serial.printf("   RAM read-back: %u %s\n", ram, ram == steps ? "(match)" : "(MISMATCH)");
        }
        if (ok) {
            ok = checkedWrite(Aim::Reg::saveFlag, 1);
            delay(200);
        }

        // Always restore output and STEP/DIR control, even after a failure.
        bool restored = checkedWrite(Aim::Reg::driveOutputEnable, origOutput);
        restored = checkedWrite(Aim::Reg::modbusEnable, origEnable) && restored;

        uint16_t after = 0;
        bool readOk = checkedRead(Aim::Reg::stepsPerRev, after);
        ok = ok && restored && readOk && after == steps;
        Serial.printf("   after: steps/rev=%u, 0x00 restored to %u (%s)\n", after, origEnable,
                      origEnable == 0 ? "STEP/DIR active" : "STEP/DIR still ignored");
        if (ok) {
            Serial.printf("RESULT: PASS (saved). Power-cycle the motor, then run `verify %u`.\n", steps);
        } else {
            Serial.println("RESULT: FAIL");
        }
    }

    void cmdVerify(long expected) {
        uint16_t steps = 0, enable = 0;
        if (!checkedRead(Aim::Reg::stepsPerRev, steps) || !checkedRead(Aim::Reg::modbusEnable, enable)) {
            Serial.println("RESULT: FAIL (no reply)");
            return;
        }
        Serial.printf("   steps/rev=%u (expected %ld), 0x00=%u\n", steps, expected, enable);
        Serial.println(steps == expected ? "RESULT: PASS - value survived the power cycle"
                                         : "RESULT: FAIL - value not persisted");
    }

    void printHelp() {
        Serial.println(R"(
Commands (numbers accept decimal or 0x hex):
  ping                 talk to the motor: steps/rev, voltage, temp, alarm
  scan                 try baud 19200/9600/38400/115200 x ID 1-5
  sniff                raw read of 0x0B with hex dump of the reply (bypasses eModbus)
  skip00               toggle ignoring a leading 0x00 glitch byte in replies
  waitmode             toggle fast notify wait vs eModbus syncRequest (for comparison)
  bus raw|emodbus      switch backend: AimBus (raw RTU master) or eModbus
  gap <us>             AimBus quiet time before each request (default 1500; motor needs >~1150)
  busstats             print + reset AimBus error counters
  dump                 read all registers 0x00-0x19
  read <reg>           read one register
  write <reg> <val>    write one register (auto-enables 0x00, reads back)
  save                 write 0x14 = 1 (persist RAM values to EEPROM)
  monitor              live voltage/current/speed/temp/pwm/position
  bench read|write|both|write78|both78 [n]
                       time n position reads / 0-step position writes / both (default 200)
                       (78 = vendor FC 0x78 move instead of FC 0x10)
  steps <n>            set + save steps/rev (0x0B) with full checks
  verify <n>           after a power cycle, confirm 0x0B == n
  unlock / lock        set 0x00 (Modbus enable) to 1 / 0 manually
  id <n>, baud <n>     change the Modbus ID / baud the tool uses
  setbaud <baud> a|b   change the MOTOR's baud (manual 5.6), takes effect after power cycle
  help)");
    }

    void handleCommand(String line) {
        line.trim();
        if (line.isEmpty()) {
            return;
        }
        int sp1 = line.indexOf(' ');
        String cmd = sp1 < 0 ? line : line.substring(0, sp1);
        String rest = sp1 < 0 ? "" : line.substring(sp1 + 1);
        rest.trim();
        int sp2 = rest.indexOf(' ');
        String arg1 = sp2 < 0 ? rest : rest.substring(0, sp2);
        String arg2 = sp2 < 0 ? "" : rest.substring(sp2 + 1);
        arg2.trim();
        cmd.toLowerCase();

        long a = 0, b = 0;
        bool hasA = parseNumber(arg1, a);
        bool hasB = parseNumber(arg2, b);

        if (cmd == "help" || cmd == "?") {
            printHelp();
        } else if (cmd == "ping") {
            cmdPing();
        } else if (cmd == "scan") {
            cmdScan();
        } else if (cmd == "sniff") {
            cmdSniff();
        } else if (cmd == "bus") {
            arg1.toLowerCase();
            if (arg1 == "raw" || arg1 == "emodbus") {
                useRawBus = arg1 == "raw";
                startBus(baud);
            }
            Serial.printf("bus backend: %s\n", useRawBus ? "AimBus (raw)" : "eModbus");
        } else if (cmd == "gap" && hasA && a >= 0 && a <= 5000) {
            aimBus.setMinGapUs(a);
            Serial.printf("AimBus inter-frame gap: %ld us\n", a);
        } else if (cmd == "busstats") {
            printBusStats();
            aimBus.resetStats();
        } else if (cmd == "waitmode") {
            notifyWait = !notifyWait;
            Serial.printf("request wait: %s\n", notifyWait ? "notify (fast)" : "eModbus syncRequest (10 ms polling)");
        } else if (cmd == "skip00") {
            skipLeadingZero = !skipLeadingZero;
            MB.skipLeading0x00(skipLeadingZero);
            Serial.printf("skip leading 0x00 byte: %s\n", skipLeadingZero ? "on" : "off");
        } else if (cmd == "dump") {
            cmdDump();
        } else if (cmd == "read" && hasA) {
            cmdRead(a);
        } else if (cmd == "write" && hasA && hasB) {
            cmdWrite(a, b);
        } else if (cmd == "save") {
            cmdSave();
        } else if (cmd == "monitor") {
            cmdMonitor();
        } else if (cmd == "setbaud" && hasA) {
            cmdSetBaud(a, arg2);
        } else if (cmd == "bench" && !arg1.isEmpty()) {
            cmdBench(arg1, hasB ? b : 200);
        } else if (cmd == "steps" && hasA) {
            cmdSteps(a);
        } else if (cmd == "verify" && hasA) {
            cmdVerify(a);
        } else if (cmd == "unlock") {
            checkedWrite(Aim::Reg::modbusEnable, 1);
        } else if (cmd == "lock") {
            checkedWrite(Aim::Reg::modbusEnable, 0);
        } else if (cmd == "id" && hasA && a >= 1 && a <= 247) {
            serverId = a;
            aimBus.setServerId(serverId);
            Serial.printf("using ID %u\n", serverId);
        } else if (cmd == "baud" && hasA && a > 0) {
            startBus(a);
            Serial.printf("using %lu baud\n", baud);
        } else {
            Serial.println("unknown command or missing argument - type `help`");
        }
    }

}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== OSSM RS485 hat / AIM57 test ===");
    FCT::redefineType(fcIncrementalMove, FCUSER);  // vendor FC, unknown to eModbus
    MB.onDataHandler(onResponse);                  // handlers must be set before begin()
    MB.onErrorHandler(onRequestError);
    startBus(Aim::defaultBaud);
    delay(100);
    cmdPing();
    printHelp();
    Serial.print("> ");
}

void loop() {
    String line = readLineBlocking(UINT32_MAX);
    handleCommand(line);
    Serial.print("> ");
}
