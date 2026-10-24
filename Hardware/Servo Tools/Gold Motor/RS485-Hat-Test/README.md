# RS485 Hat Test (AIM57 / Gold Motor)

Standalone ESP32 firmware for bringing up the RS485 hat on an OSSM V2.3 board. It talks Modbus RTU to the
57AIM30 Gold Motor so you can check comms, read and write every parameter, and set steps/rev.
This is separate from the main OSSM firmware in `/Software`.

The Modbus layer uses the [eModbus](https://github.com/eModbus/eModbus) library (`ModbusClientRTU`). The ESP32 UART runs in
hardware RS485 half-duplex mode, so the UART drives the transceiver's DE/~RE from its RTS pin. No GPIO toggling in software.

## Wiring (V2.3 + hat)

| Hat | V2.3 | Use |
|---|---|---|
| J1 | J6 | 3V3 |
| J2-1 | J9 IO2 | DE/~RE (high = transmit), 10k pull-down |
| J2-2 | J9 IO15 | not connected (strapping pin) |
| J2-3 | J9 IO33 | UART RX (from transceiver R) |
| J2-4 | J9 IO22 | UART TX (to transceiver D) |
| J3 | J7 | GND |
| J5 | motor RS485 port | 1 = +5V, 2 = A, 3 = B, 4 = GND |

On the motor's 10-pin communication port: pin 2 = 485A, pin 3 = 485B, pin 6 = COM (485 ground), pin 10 = 485_5V.
The motor's RS485 side is opto-isolated and **needs 5V from the controller**, so J5-1 must be connected.
The motor also needs its own 20–36 V supply (green LED on).

## Build / flash

```
pio run -e rs485_hat_v23 -t upload -t monitor     # V2.3 + hat: TX 22, RX 33, DE 2
pio run -e ossm_v25      -t upload -t monitor     # V2.5 on-board: TX 17, RX 16, DE 32
```

## Bench procedure

1. Power the motor, flash, open the monitor (115200). `ping` runs on boot:
   `OK in 12 ms: steps/rev 800, 24.1 V, 30 C, alarm 0x00 (none)`.
   On failure it prints a hint (timeout, garbled reply, or rejected request).
2. `scan` if nothing answers (tries 19200/9600/38400/115200 × ID 1–5).
3. `dump` to see all 26 registers with scaled values. Save the output as a backup of the motor's settings.
4. `steps 800`. It enables Modbus writes, turns output off, writes 0x0B, reads it back, saves, then restores 0x01 and 0x00.
   The motor goes limp for about a second, so keep the rail unloaded.
5. Power-cycle **the motor**, then run `verify 800` → `RESULT: PASS` means the value is in EEPROM.

## Commands

| Command | What it does |
|---|---|
| `ping` | Steps/rev, voltage, temperature, alarm + round-trip time |
| `scan` | Find the motor's baud/ID |
| `dump` | All registers 0x00–0x19, plus absolute position as int32 |
| `read <reg>` | One register |
| `write <reg> <val>` | Write with read-back. Enables 0x00 for the write, then restores it |
| `save` | 0x14 = 1 (persist to EEPROM) |
| `monitor` | Live V / A / rpm / °C / PWM / position / alarm every 500 ms |
| `steps <n>` / `verify <n>` | Set + save steps/rev, then confirm after a power cycle |
| `unlock` / `lock` | 0x00 = 1 / 0 by hand |
| `id <n>` / `baud <n>` | Change the ID / baud the tool uses |

Numbers accept decimal or `0x` hex.

## Bench findings (V2.3 + hat, 2026-10-23)

- Comms work at the default 19200 / ID 1, with a ~20 ms round trip for `ping`.
- **Every reply starts with a stray `0x00` byte** (glitch as the transceiver switches back to receive), e.g.
  `00 01 03 02 03 20 B9 6C`. eModbus rejects it as a CRC error unless `skipLeading0x00` is on, which the tool now
  enables by default. `skip00` toggles it and `sniff` shows the raw bytes. The production firmware will need the same
  handling, or a hardware fix (e.g. fail-safe bias resistors on A/B).

### Position throughput (`bench`, 19200 baud)

`bench write` sends an incremental move of 0 steps (FC 0x10 to 0x0C). It's the same frame as a real position
command, but the motor doesn't move. Every run aborts if the position drifts more than 20 counts.

| Test | Avg per transaction | Rate | Wire floor |
|---|---|---|---|
| Position read (FC03, 0x16–0x17) | 13.0 ms | 77 Hz | 8.9 ms |
| Position write (FC10, 0x0C–0x0D) | 15.0 ms | 67 Hz | 10.9 ms |
| Write + read cycle | 28.0 ms | 36 Hz | 19.8 ms |
| Position write, vendor FC 0x78 (`write78`) | 13.0 ms | 77 Hz | 8.3 ms |
| FC 0x78 write + read cycle (`both78`) | 26.0 ms | 38 Hz | 17.2 ms |

### At 115200 baud

| Test | Avg per transaction | Rate | Wire floor |
|---|---|---|---|
| Position read (FC03) | 5.9 ms | 168 Hz | 1.5 ms |
| Position write (FC10) | 6.0 ms | 167 Hz | 1.8 ms |
| Position write (FC 0x78) | 5.0 ms | **200 Hz** | 1.4 ms |
| FC10 write + read | 11.9 ms | 84 Hz | 3.3 ms |
| FC 0x78 write + read | 10.9 ms | **92 Hz** | 2.9 ms |

0 failures over 3,550 transactions, and position didn't move. Wire time is now only ~25% of each transaction. The rest
is eModbus timing: a fixed 1.75 ms inter-frame gap above 19200 baud, and 1 ms `delay(1)` polling while it waits for the
first byte and for the end of the frame.

### AimBus (our raw RTU master) at 115200

`bus raw` switches every command to `lib/AimBus`. It writes frames straight to the UART and wakes on the UART's
RX-idle timeout (2 character times) instead of polling. Framing and CRC live in `lib/AimFrame` (no Arduino
dependencies). It's covered by 15 unit tests, which use the manual's worked examples as vectors:
`pio test -e rs485_hat_v23 -f test_aim_frame`.

| Test | eModbus | **AimBus** | Wire floor |
|---|---|---|---|
| Position read (FC03) | 5.9 ms / 168 Hz | **4.2 ms / 241 Hz** | 1.5 ms |
| Position write (FC10) | 6.0 ms / 167 Hz | **4.5 ms / 224 Hz** | 1.8 ms |
| Position write (FC 0x78) | 5.0 ms / 200 Hz | **4.1 ms / 247 Hz** | 1.4 ms |
| FC10 write + read | 11.9 ms / 84 Hz | **8.6 ms / 116 Hz** | 3.3 ms |
| FC 0x78 write + read | 10.9 ms / 92 Hz | **8.2 ms / 122 Hz** | 2.9 ms |

Soak: 37,380 transactions with 0 timeouts, 0 CRC errors and 0 retries, plus 10,000 more read-only.

**The motor needs ~1.15 ms of quiet after its reply.** A request that starts sooner is silently ignored. The gap
sweep showed every other frame dropped at ≤1000 µs, 1 in 1005 at 1150 µs, and 0 at ≥1200 µs. AimBus defaults to
1500 µs (`gap <us>` to change). This gap is now the largest fixed cost per transaction, bigger than the wire time.

**Position creep with repeated 0-step FC 0x78 moves (unresolved).** In the 20k-cycle soak the position crept
+21 counts over ~150 s and tripped the 20-count safety stop. With read-only traffic it stayed exact (0 drift over
10,000 reads), and it went back to the original position once 0x00 returned to 0. Likely cause: incremental moves are
relative to the *actual* position, so following error accumulates into the target. Streaming motion should use
absolute targets (0x16/0x17 via FC10, or vendor FC 0x7B). Their units still need confirming before anything moves.

**Changing the motor's baud (manual §5.6) — confirmed working as "variant a":** `setbaud 115200 a` writes
0x00=1, 0x03=803, 0x04=129, 0x00=506. Power-cycle the motor and it answers at 115200. Codes: 800 = 9600, 801 = 19200,
802 = 38400, 803 = 115200. Nothing needs saving: after the power cycle 0x03/0x04 read back their normal values.
To go back: `setbaud 19200 a`, then power-cycle. The tool boots at 19200, so after a reset run `baud 115200`
(or `scan`) first. The GMP web tool only talks 19200.

FC 0x78 sends an incremental move as 4 signed bytes, most significant byte first, in an 8-byte frame (vs 13 bytes for FC10).
eModbus needs `FCT::redefineType(0x78, FCUSER)` to send it. A 0-step move replies `01 78 00 09 00 00`.

0 failures over 1,300 transactions, and position stayed within ±1 count. The ~4 ms over the wire floor per transaction
is the Modbus 3.5-character gap (~1.8 ms), the motor's turnaround time, and 1 ms task ticks.

eModbus' `syncRequest()` polls every 10 ms, which caps every transaction at 20 ms (50 Hz). The tool uses
`addRequest()` + a task notification instead (`waitmode` switches back for comparison). Production code should do the same.

| Command | What it does |
|---|---|
| `bench read\|write\|both [n]` | Time position reads / 0-step position writes / both |
| `waitmode` | Toggle fast notify wait vs eModbus `syncRequest` |
| `sniff` | Raw FC03 read of 0x0B with a hex dump of the reply, bypassing eModbus |
| `skip00` | Toggle skipping a leading 0x00 byte (on by default) |

## Things the manual says that matter

- **0x00 = 1 is required to write any other parameter, and while it is 1 the motor ignores STEP/DIR.** The tool always puts
  0x00 back afterwards. If a session dies halfway, `lock` or a motor power cycle restores pulse control.
- **0x0A (gear numerator) = 0** makes the motor power up in Modbus mode. **0x19 (special function) ≠ 0** runs a
  homing/other mode at power-up. Writing either asks for confirmation, and so does writing 0x04, 0x08, 0x16 or 0x17.
- Read-only over FC 0x06: 0x0C/0x0D (target position, FC 0x10 only), 0x0E–0x13 (telemetry), 0x15 (address, FC 0x7A only).
- Scaling: current = x/2000 A, speed = signed x/10 rpm, voltage = x/327 V, temperature in °C, PWM = signed ±32768 → ±100%,
  max output 0–609 = 0–60.9%.
- The manual's ranges don't always match shipped motors (GMP defaults use 0x01 = 7 and 0x04 = 495). Out-of-range writes
  ask for confirmation instead of being refused.

Full register table: `src/AimRegisters.h`. Source: `../YZ-AIMManual_v2_55.pdf` section V.
