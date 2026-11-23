# CLAUDE.md - OSSM Firmware

## Overview

OSSM (Open Source Sex Machine) is an ESP32-based stepper motor device firmware. It drives a linear actuator with pattern-based motion control, BLE remote control, an OLED display, and rotary encoder UI. Built with PlatformIO, Arduino framework, and Boost.SML state machine. Communicates with the RAD Dashboard via BLE and WiFi.

## Tech Stack

- **MCU:** ESP32 (esp32dev, platform espressif32@6.3.2)
- **Framework:** Arduino + FreeRTOS
- **C++ Standard:** C++17 (gnu++17)
- **State Machine:** Boost.SML (header-only, `include/boost/sml.hpp`)
- **Motor:** FastAccelStepper (stepper motor with trapezoidal acceleration)
- **Display:** U8g2 (128x64 SSD1306 OLED)
- **BLE:** NimBLE-Arduino 2.1.2
- **LED:** FastLED (WS2812B status indicator)
- **Build:** PlatformIO with 5 environments (development, developmentAJ, staging, production, test)

## Quick Commands

```bash
pio run -e development            # Build development
pio run -e development -t upload  # Build + upload
pio run -e development -t monitor # Serial monitor (115200 baud)
pio run -e test -t test           # Run unit tests (native platform)
pio check                         # Run clang-tidy static analysis
```

## Project Structure

```
Software/
├── platformio.ini               # Build config (5 environments)
├── pre_build_script.py          # Build-time version injection
├── src/
│   ├── main.cpp                 # Entry point, FreeRTOS task creation
│   ├── ossm/                    # Core state machine & operation modes
│   │   ├── OSSM.h               # Main class + Boost.SML state machine (~515 lines)
│   │   ├── OSSM.cpp             # Constructor, display tasks
│   │   ├── OSSM.StrokeEngine.cpp    # Pattern-based motion control loop
│   │   ├── OSSM.SimplePenetration.cpp # Linear back-and-forth mode
│   │   ├── OSSM.Homing.cpp     # Home switch calibration
│   │   ├── OSSM.PlayControls.cpp    # STROKE→DEPTH→SENSATION cycling
│   │   ├── OSSM.PatternControls.cpp # Pattern selection UI
│   │   ├── OSSM.Menu.cpp       # Menu navigation
│   │   ├── OSSM.WiFi.cpp       # WiFi portal
│   │   ├── OSSM.Update.cpp     # OTA firmware updates
│   │   ├── OSSM.Preflight.cpp  # Safety checks (knob at zero)
│   │   └── OSSM.Help.cpp       # Help screen
│   ├── services/                # Hardware abstraction
│   │   ├── board.h/cpp          # Board init (GPIO, stepper, encoder, LED)
│   │   ├── stepper.h/cpp        # FastAccelStepper motor init
│   │   ├── display.h/cpp        # OLED display (mutex-protected)
│   │   ├── encoder.h/cpp        # Rotary encoder (interrupt-driven)
│   │   ├── led.h/cpp            # FastLED status indicators
│   │   ├── wm.h/cpp             # WiFiManager
│   │   └── communication/
│   │       ├── nimble.cpp       # BLE server implementation
│   │       └── queue.h          # Streaming position queue
│   ├── constants/
│   │   ├── Pins.h               # All GPIO pin definitions
│   │   ├── Config.h             # System parameters
│   │   ├── Menu.h               # Menu options enum
│   │   └── UserConfig.h         # Language, metric, knob config
│   ├── command/
│   │   └── commands.hpp         # BLE command parsing
│   ├── structs/
│   │   └── SettingPercents.h    # Global settings (speed, stroke, depth, sensation, pattern)
│   ├── utils/
│   │   ├── RecursiveMutex.h     # Thread-safe state machine mutex
│   │   ├── StateLogger.h        # Boost.SML debug logger
│   │   ├── StrokeEngineHelper.h # Sensation calc, deadzone detection
│   │   ├── analog.h             # Potentiometer smoothing
│   │   └── ble.h                # BLE utilities
│   └── extensions/              # Library extensions
├── lib/
│   └── StrokeEngine/            # Custom motion control library
│       └── src/
│           ├── StrokeEngine.h   # Motion engine API (~438 lines)
│           └── pattern.h        # Pattern base class
├── include/
│   └── boost/sml.hpp            # State machine library
└── test/                        # Unit tests (Unity + ArduinoFake)
```

## Hardware Pin Map

| Function          | Pin(s)  | Notes                          |
|-------------------|---------|--------------------------------|
| STEP signal       | 14      | Stepper pulse output           |
| DIR signal        | 27      | Stepper direction              |
| Motor Enable      | 26      | Stepper enable (active low)    |
| Home Switch       | 12      | Homing/calibration endstop     |
| Display SCL       | 19      | I2C clock (shared with E-stop) |
| Display SDA       | 21      | I2C data                       |
| RGB LED           | 25      | WS2812B status LED             |
| Speed Pot (ADC)   | 34      | Analog speed knob              |
| Current Sensor    | 36      | Analog current monitoring      |
| Encoder Switch    | 35      | Rotary encoder button          |
| Encoder A         | 18      | Rotary encoder phase A         |
| Encoder B         | 5       | Rotary encoder phase B         |
| WiFi Reset        | 23      | WiFi config reset button       |
| Expansion         | 2, 15, 22, 33 | Unused GPIO              |

## Key Patterns

### State Machine (Boost.SML)

The core logic lives in `src/ossm/OSSM.h`. States flow through:

```
idle → homing → menu → operation modes → error
```

- **Operation Modes:** `simplePenetration`, `strokeEngine`, `streaming`
- **Support States:** `update`, `wifi`, `error`, `help`
- **Thread Safety:** `sml::thread_safe<ESP32RecursiveMutex>`
- **Logging:** `StateLogger` traces all events, guards, actions, and state changes
- Guards validate stroke length and speed knob position before transitions

### FreeRTOS Tasks

| Task           | Priority | Purpose                                   |
|----------------|----------|-------------------------------------------|
| Button polling | High     | 25ms interval input processing            |
| NimBLE init    | Normal   | Deferred BLE start (waits for menu.idle)  |
| Motor control  | Normal   | Separate task per operation mode           |
| Display header | Normal   | Status bar updates                        |

### Motor Control

Three operation modes:

**StrokeEngine Mode** — 7 built-in patterns:
- SimpleStroke, TeasingPounding, RoboStroke, HalfnHalf, Deeper, StopNGo, Insist
- Speed: BLE 0-100% × 3 = strokes per minute
- Stroke/Depth: percentage → converted to mm via calibrated stroke length
- Sensation: pattern-specific modifier (-100 to 100)

**SimplePenetration Mode** — Pure linear oscillation, no pattern logic

**Streaming Mode** — Real-time position from BLE (`stream:POS:TIME`)

**Queued moves (`q:`, `r:`)** — binary commands on the command
characteristic, streaming mode only. Queued positions are 16-bit (0-65535
across the stroke window, 65535 = depth end) but travel as one byte each,
decoded within the current range as `min + (max - min) * byte / 255`.
- `r:` + min u16 LE + max u16 LE + up to 8 byte pairs `[position byte,
  time 0-255 ms]` (22 bytes, one default-MTU write) sets the range and
  queues those moves together; if they don't fit, the range doesn't change
  either (`fail:r`). Clients send each batch with its own range, so slow
  moves keep fine resolution (a single 0-255 range across the stroke made
  slow moves stall on repeated values).
- `q:` + up to 10 byte pairs queues moves in the last `r:` range. The range
  resets to 0-65535 when streaming mode starts.

Moves are decoded on arrival into a 32-slot ring buffer
(`lib/OSSMLogic/src/queued_moves.h`) that the streaming task drains ahead of
`stream:` targets. A move ends when its time is up, whether or not it reached
its position; the next starts at that end time. The motor doesn't stop at
each point: every tick it is aimed a short lookahead (20-150 ms: long enough
not to brake mid-stroke, and at least 8 steps ahead so slow moves don't
jitter on step rounding) along a monotone cubic curve through the started and
buffered points (`lib/OSSMLogic/src/trajectory.h`), clamped to the stroke
window, at the speed that gets there on time (at least 10 steps/s). The curve
never overshoots a point and eases to a stop on the last one if the buffer
runs dry. A command that doesn't fully fit is rejected whole (`fail:q`).
`q:` with no bytes clears the buffer, drops pending `stream:` targets, and
decelerates to a stop. Both are parsed on the raw bytes in `command.hpp`
`onWrite`, before the regex/String path, which would truncate at 0x00.
`set:lowwater:n` (0-65535 ms, default 500, not persisted) is how much queued
time the device aims to keep; the status packet's last byte reports when it
falls short (see Status below).

### StrokeEngine Library (`lib/StrokeEngine/`)

Custom motion control library with its own state machine:

```
UNDEFINED → READY → PATTERN / SETUPDEPTH / STREAMING
```

- Trapezoidal acceleration profiles
- Machine geometry: `physicalTravel` (mm) + `keepoutBoundary` (mm)
- Motor properties: `maxSpeed`, `maxAcceleration`, `stepsPerMillimeter`

### BLE Protocol

- **Service UUID:** `522b443a-4f53-534d-0001-420badbabe69`
- **Characteristics:**
  - Command (0x1000) — Write
  - State (0x2000) — Read/Notify, JSON. Shared with the RAD BLE library,
    which adds its own fields; the RAD Dashboard depends on it.
  - Status (0x2200) — Read/Notify, 9-byte binary that fits one ATT packet:
    `[state u8][speed u8][stroke u8][depth u8][position u16 0.1 mm from
    home][timestamp u16 ms since previous notification][lowWater u8]`,
    little-endian. lowWater is 0 when the queued moves plus the rest of the
    running one last at least the low-water time, else the free slot count
    (always 0 outside streaming mode).
    Notified like the JSON: when the state or a setting changes, plus a 1 s
    heartbeat; position alone never triggers one, to keep BLE traffic down.
    Also notified when a queued move completes below the low-water mark, at
    most once per 100 ms (a pending one waits for the window, and is dropped
    if the buffer has refilled by then).
    Reads return live values with delta 0. State numbers are
    `StateName::Value` in `lib/OSSMLogic/src/status_packet.h` (255 =
    Unknown): append only, never renumber. 2010/2100/2300 are RAD BLE's.
  - Patterns (0x3000) — Read
  - GPIO (0x4000) — Read
- **Command Format:** `go:{mode}`, `set:{param}:{value}`, `stream:{pos}:{time}`
- **Safety:** Speed ramps down over 2s on BLE disconnect

### Settings (SettingPercents)

```cpp
struct SettingPercents {
    float speed;       // 0-100%
    float stroke;      // 0-100%
    float sensation;   // 0-100%
    float depth;       // 0-100%
    StrokePatterns pattern;
    float speedKnob;   // Physical pot reading
    std::optional<float> speedBLE; // BLE override
};
```

### Safety Features

- **Preflight check:** Speed knob must be at zero before operation starts
- **Emergency stop:** Long button press kills motor immediately
- **BLE disconnect:** Speed ramps down over 2s (not instant stop)
- **Keepout boundaries:** Soft endstops prevent travel beyond safe range
- **Home switch:** Calibration required before any operation mode

## Build Configuration

### Environments

| Env           | Debug | Platform         | Notable Flags                        |
|---------------|-------|------------------|--------------------------------------|
| development   | 4     | espressif32@6.3.2| `DEBUG_TALKATIVE`, `PRETEND_TO_BE_FLESHY_THRUST_SYNC` |
| developmentAJ | 4     | espressif32      | Custom hardware, hardcoded WiFi      |
| staging       | 1     | espressif32      | `PRETEND_TO_BE_FLESHY_THRUST_SYNC`   |
| production    | 0     | espressif32      | `PRETEND_TO_BE_FLESHY_THRUST_SYNC`   |
| test          | 0     | native           | `UNITY_INCLUDE_DOUBLE`, ArduinoFake  |

### Build Script

`pre_build_script.py` runs before compilation:
- Sets `SW_VERSION` from environment or GITHUB_ENV

### Flash Partitions

Uses `min_spiffs.csv` (PlatformIO built-in minimal SPIFFS partition).

## Libraries

| Library                        | Version  | Purpose                    |
|--------------------------------|----------|----------------------------|
| NimBLE-Arduino                 | ^2.1.2   | BLE stack                  |
| FastAccelStepper               | ^0.30.13 | Stepper motor control      |
| U8g2                           | ^2.35.8  | OLED display (SSD1306)     |
| FastLED                        | ^3.6.0   | RGB LED status             |
| ArduinoJson                    | latest   | JSON parsing               |
| QRCode                         | ^0.0.1   | QR code generation         |
| Ai Esp32 Rotary Encoder        | ^1.6     | Rotary encoder             |
| OneButton                      | ^2.5.0   | Button debouncing          |
| WiFiManager (git)              | latest   | WiFi config portal         |
| ArduinoFake                    | ^0.4.0   | Test mocking (test only)   |

## Testing

### Unit tests (native, no hardware)

- Framework: Unity (PlatformIO native)
- Mocking: ArduinoFake
- Run: `pio test -e test`

### Hardware tests (on-device, USB-connected OSSM)

- Environment: `hw_test` (extends `development`, builds full `src/`)
- Run all: `pio test -e hw_test`
- Run one: `pio test -e hw_test -f test_hw_smoke`
- Available suites: `test_hw_smoke`, `test_hw_homing`, `test_hw_homing_error`, `test_hw_pairing`, `test_hw_state_machine`, `test_hw_wifi`
- `test_hw_homing` moves the motor — keep the rail clear

## Important Rules

- Always put `{` and `}` around the body of `if`, `else`, `for` and `while`,
  even when it is a single line
- Never commit credentials or WiFi passwords
- State machine (`OSSM.h`) is the single source of truth for device behavior
- Display access must use mutex protection
- Homing calibration must complete before any operation mode can start
- Speed knob preflight check is a safety requirement — don't bypass it
- BLE disconnect must always trigger gradual ramp-down, never instant stop
- StrokeEngine keepout boundaries are safety-critical — respect them

## Workflow

- Never commit changes automatically. Let me review and commit manually.
- Before making any code edit, explicitly state what the change does, what the side effects are, and what areas are affected. Wait for explicit approval before editing any file.
- Be opinionated but explain reasoning.
- If you learn something new about this project, add it to this file.
- Always verify state machine transitions when modifying device behavior.
