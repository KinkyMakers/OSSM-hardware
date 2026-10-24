#include "SineTest.h"

#include <esp_heap_caps.h>
#include <math.h>

#include "AimRegisters.h"

namespace {

    constexpr float maxStrokeMm = 100;
    constexpr float softLimitMm = 52;  // commands beyond centre +/- this abort the run
    constexpr float fadeInS = 2.0f;
    constexpr float fadeOutS = 1.5f;
    constexpr size_t maxSamples = 6000;
    constexpr uint16_t teleFirst = Aim::Reg::alarmCode;  // 0x0E..0x17 in one read
    constexpr uint16_t teleCount = Aim::Reg::absPositionHigh - teleFirst + 1;

    struct Sample {
        uint32_t tUs;
        int32_t cmd;  // counts, sine evaluated at the read time (no lookahead)
        int32_t act;  // counts
        int16_t current;  // raw, A = x / 2000
        int16_t speed;    // raw, rpm = x / 10
        uint16_t fmHz;    // instantaneous frequency in mHz
    };

    // Log chirp f(t) = f0 * k^(t/T), held at fCap once reached. Phase is closed-form so send jitter can't distort it.
    struct Chirp {
        float f0, T, lnk, fCap, tCap, phaseAtCap;

        Chirp(float f0_, float f1, float T_, float fCap_) : f0(f0_), T(T_) {
            lnk = logf(f1 / f0);
            fCap = fminf(fCap_, f1);
            tCap = (fabsf(lnk) < 1e-6f || fCap >= f1) ? T : T * logf(fCap / f0) / lnk;
            phaseAtCap = rawPhase(tCap);
        }

        float rawPhase(float t) const {
            if (fabsf(lnk) < 1e-6f) {
                return 2 * PI * f0 * t;
            }
            return 2 * PI * f0 * T / lnk * (expf(lnk * t / T) - 1);
        }

        float phase(float t) const { return t <= tCap ? rawPhase(t) : phaseAtCap + 2 * PI * fCap * (t - tCap); }

        float freq(float t) const { return t <= tCap ? f0 * expf(lnk * t / T) : fCap; }
    };

    float envelope(float t, float T) {
        if (t <= 0 || t >= T) {
            return 0;
        }
        if (t < fadeInS) {
            return 0.5f * (1 - cosf(PI * t / fadeInS));
        }
        if (t > T - fadeOutS) {
            return 0.5f * (1 - cosf(PI * (T - t) / fadeOutS));
        }
        return 1;
    }

    int32_t toInt32(uint16_t low, uint16_t high) {
        return static_cast<int32_t>((static_cast<uint32_t>(high) << 16) | low);
    }

    bool readPosition(AimBus &bus, int32_t &pos) {
        uint16_t w[2];
        if (bus.readRegisters(Aim::Reg::absPositionLow, 2, w) != AimBus::Result::Ok) {
            return false;
        }
        pos = toInt32(w[0], w[1]);
        return true;
    }

    bool write(AimBus &bus, uint16_t reg, uint16_t value) {
        AimBus::Result r = bus.writeRegister(reg, value);
        if (r != AimBus::Result::Ok) {
            Serial.printf("   write 0x%02X = %u FAILED: %s\n", reg, value, AimBus::resultText(r));
            return false;
        }
        return true;
    }

}

bool parseSineParams(const String &args, SineParams &p, String &error) {
    float v[10];
    int n = 0;
    String rest = args;
    rest.trim();
    while (rest.length() && n < 10) {
        int sp = rest.indexOf(' ');
        String tok = sp < 0 ? rest : rest.substring(0, sp);
        rest = sp < 0 ? "" : rest.substring(sp + 1);
        rest.trim();
        char *end = nullptr;
        v[n] = strtof(tok.c_str(), &end);
        if (!end || *end) {
            error = "not a number: " + tok;
            return false;
        }
        n++;
    }
    if (n > 0) p.strokeMm = v[0];
    if (n > 1) p.f0 = v[1];
    if (n > 2) p.f1 = v[2];
    if (n > 3) p.seconds = v[3];
    if (n > 4) p.accel = static_cast<uint16_t>(v[4]);
    if (n > 5) p.rateHz = v[5];
    if (n > 6) p.readEvery = static_cast<uint16_t>(v[6]);
    if (n > 7) p.rpmCap = v[7];
    if (n > 8) p.lookaheadMs = v[8];
    if (n > 9) p.followMm = v[9];

    if (p.strokeMm <= 0 || p.strokeMm > maxStrokeMm) {
        error = "stroke must be 0-100 mm";
    } else if (p.f0 <= 0 || p.f1 <= 0 || p.f0 > 10 || p.f1 > 10) {
        error = "frequencies must be 0-10 Hz";
    } else if (p.seconds < fadeInS + fadeOutS + 1 || p.seconds > 120) {
        error = "seconds must be 5-120";
    } else if (p.readEvery < 1) {
        error = "read_every must be >= 1";
    } else if (p.rpmCap <= 0 || p.rpmCap > 1500) {
        error = "rpm_cap must be 0-1500";
    } else if (p.rateHz < 0) {
        error = "rate must be >= 0";
    } else if (p.followMm <= 0 || p.followMm > 30) {
        error = "follow_mm must be 0-30";
    } else {
        return true;
    }
    return false;
}

void runSine(AimBus &bus, const SineParams &p) {
    // ---- setup: record state, derive units
    uint16_t orig[4] = {};  // 0x00..0x03
    int32_t centre = 0;
    if (bus.readRegisters(Aim::Reg::modbusEnable, 4, orig) != AimBus::Result::Ok || !readPosition(bus, centre)) {
        Serial.println("sine aborted: couldn't read motor state");
        return;
    }
    const uint32_t countsPerRev = Aim::encoderCountsPerRev;  // not 0x0B: that gear only applies to STEP/DIR
    const float countsPerMm = countsPerRev / p.mmPerRev;
    const float ampMm = p.strokeMm / 2;
    const float vCapMmS = p.rpmCap / 60 * p.mmPerRev;
    const float fCap = vCapMmS / (2 * PI * ampMm);
    const Chirp chirp(p.f0, p.f1, p.seconds, fCap);
    const float lookaheadS = p.lookaheadMs / 1000;
    const int32_t softLimit = lroundf(softLimitMm * countsPerMm);
    const int32_t followLimit = lroundf(p.followMm * countsPerMm);

    Serial.printf("sine: %.0f mm stroke, %.2f -> %.2f Hz over %.0f s, accel=%u, rate=%s, read every %u, "
                  "cap %.0f rpm (= %.2f Hz), lookahead %.1f ms, abort above %.0f mm error\n",
                  p.strokeMm, p.f0, p.f1, p.seconds, p.accel, p.rateHz > 0 ? String(p.rateHz, 0).c_str() : "max",
                  p.readEvery, p.rpmCap, fCap, p.lookaheadMs, p.followMm);
    Serial.printf("   units: %lu encoder counts/rev = %.2f counts/mm; centre %ld; was 0x00=%u 0x02=%u 0x03=%u\n",
                  static_cast<unsigned long>(countsPerRev),
                  countsPerMm, static_cast<long>(centre), orig[0], orig[2], orig[3]);
    if (fCap < p.f1) {
        Serial.printf("   frequency will stop rising at %.2f Hz (peak speed cap)\n", fCap);
    }

    // Size the log to what the heap can give in one block (keeping some headroom), up to maxSamples.
    const size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    const size_t capacity = min(maxSamples, (largest > 16384 ? largest - 16384 : 0) / sizeof(Sample));
    auto *log = capacity >= 200 ? static_cast<Sample *>(malloc(capacity * sizeof(Sample))) : nullptr;
    if (!log) {
        Serial.printf("sine aborted: no memory for log (largest free block %u bytes)\n", largest);
        return;
    }
    Serial.printf("   log capacity %u samples\n", capacity);
    size_t logCount = 0;

    // Start holding the current position before taking Modbus control, so nothing jumps.
    bool ok = write(bus, Aim::Reg::modbusEnable, 1) && bus.moveTo(centre) == AimBus::Result::Ok &&
              write(bus, 0x02, 1500) && write(bus, 0x03, p.accel);
    while (Serial.available()) {
        Serial.read();
    }

    // ---- stream
    const char *abortReason = ok ? nullptr : "setup failed";
    uint32_t writes = 0, writeErrors = 0, reads = 0, readErrors = 0, consecutiveErrors = 0;
    float maxFreq = 0, sumSq = 0, peakErr = 0;
    uint32_t errSamples = 0;
    int32_t lastActual = centre;
    const uint32_t t0 = micros();
    uint32_t cycle = 0;
    float t = 0;

    while (!abortReason) {
        if (p.rateHz > 0) {
            uint32_t slot = t0 + static_cast<uint32_t>(cycle * 1e6f / p.rateHz);
            while (static_cast<int32_t>(micros() - slot) < 0) {
            }
        }
        const uint32_t nowUs = micros() - t0;
        t = nowUs / 1e6f;
        if (t >= p.seconds) {
            break;
        }
        if (Serial.available()) {
            abortReason = "key pressed";
            break;
        }

        // One telemetry read after every readEvery writes (readEvery=1 alternates write/read).
        const bool isRead = (cycle % (p.readEvery + 1)) == p.readEvery;
        if (isRead) {
            uint16_t tele[teleCount];
            AimBus::Result r = bus.readRegisters(teleFirst, teleCount, tele);
            reads++;
            if (r != AimBus::Result::Ok) {
                readErrors++;
                consecutiveErrors++;
            } else {
                consecutiveErrors = 0;
                const float tr = (micros() - t0) / 1e6f;
                const int32_t cmdNow =
                    centre + lroundf(ampMm * countsPerMm * envelope(tr, p.seconds) * sinf(chirp.phase(tr)));
                const int32_t act = toInt32(tele[Aim::Reg::absPositionLow - teleFirst],
                                            tele[Aim::Reg::absPositionHigh - teleFirst]);
                lastActual = act;
                const float errMm = (cmdNow - act) / countsPerMm;
                sumSq += errMm * errMm;
                peakErr = fmaxf(peakErr, fabsf(errMm));
                errSamples++;
                if (logCount < capacity) {
                    log[logCount++] = {micros() - t0, cmdNow, act,
                                       static_cast<int16_t>(tele[Aim::Reg::current - teleFirst]),
                                       static_cast<int16_t>(tele[Aim::Reg::speed - teleFirst]),
                                       static_cast<uint16_t>(chirp.freq(tr) * 1000)};
                }
                if (tele[0] != 0) {
                    abortReason = "motor alarm";
                } else if (abs(cmdNow - act) > followLimit) {
                    abortReason = "following error limit";
                }
            }
        } else {
            const float tc = t + lookaheadS;
            const int32_t target =
                centre + lroundf(ampMm * countsPerMm * envelope(tc, p.seconds) * sinf(chirp.phase(tc)));
            if (abs(target - centre) > softLimit) {
                abortReason = "soft limit";
                break;
            }
            AimBus::Result r = bus.moveTo(target);
            writes++;
            if (r != AimBus::Result::Ok) {
                writeErrors++;
                consecutiveErrors++;
            } else {
                consecutiveErrors = 0;
            }
        }
        if (consecutiveErrors >= 3) {
            abortReason = "3 bus errors in a row";
        }
        maxFreq = fmaxf(maxFreq, chirp.freq(t));
        cycle++;
    }
    const float runS = (micros() - t0) / 1e6f;

    // ---- teardown: hold, return to centre gently, restore registers
    if (abortReason) {
        int32_t here = lastActual;
        readPosition(bus, here);
        bus.moveTo(here);  // hold where we are
    }
    // Slow internal trapezoid back to centre (no-op if already there).
    write(bus, 0x02, 200);
    write(bus, 0x03, 5000);
    bus.moveTo(centre);
    int32_t endPos = centre;
    for (uint32_t start = millis(); millis() - start < 4000;) {
        if (readPosition(bus, endPos) && abs(endPos - centre) <= countsPerMm / 2) {
            break;
        }
        delay(20);
    }
    write(bus, 0x03, orig[3]);
    write(bus, 0x02, orig[2]);
    write(bus, Aim::Reg::modbusEnable, orig[0]);

    // ---- report
    if (abortReason) {
        Serial.printf("   ABORTED at t=%.2f s (f=%.2f Hz): %s\n", runS, chirp.freq(runS), abortReason);
    }
    Serial.printf("   %u writes (%u failed) + %u reads (%u failed) in %.2f s = %.0f writes/s\n", writes, writeErrors,
                  reads, readErrors, runS, writes / runS);
    Serial.printf("   max frequency %.2f Hz; tracking error RMS %.2f mm, peak %.2f mm (%u samples)\n", maxFreq,
                  errSamples ? sqrtf(sumSq / errSamples) : 0, peakErr, errSamples);
    Serial.printf("   end position %.2f mm from centre; registers restored to 0x00=%u 0x02=%u 0x03=%u\n",
                  (endPos - centre) / countsPerMm, orig[0], orig[2], orig[3]);

    Serial.println("#BEGIN SINE");
    Serial.printf("# stroke=%.0f f0=%.2f f1=%.2f seconds=%.0f accel=%u rate=%.0f read_every=%u rpm_cap=%.0f "
                  "lookahead_ms=%.1f follow_mm=%.0f writes_per_s=%.1f counts_per_mm=%.3f aborted=%s\n",
                  p.strokeMm, p.f0, p.f1, p.seconds, p.accel, p.rateHz, p.readEvery, p.rpmCap, p.lookaheadMs,
                  p.followMm, writes / runS, countsPerMm, abortReason ? String(abortReason).c_str() : "no");
    Serial.println("t_ms,f_hz,cmd_mm,act_mm,err_mm,current_a,speed_rpm");
    for (size_t i = 0; i < logCount; i++) {
        const Sample &s = log[i];
        Serial.printf("%.1f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f\n", s.tUs / 1000.0f, s.fmHz / 1000.0f,
                      (s.cmd - centre) / countsPerMm, (s.act - centre) / countsPerMm, (s.cmd - s.act) / countsPerMm,
                      s.current / 2000.0f, s.speed / 10.0f);
    }
    Serial.println("#END SINE");
    free(log);
}
