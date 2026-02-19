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
  - State (0x2000) — Read/Notify
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

- Framework: Unity (PlatformIO native)
- Mocking: ArduinoFake
- Run: `pio test -e test`

## Important Rules

- Never commit credentials or WiFi passwords
- State machine (`OSSM.h`) is the single source of truth for device behavior
- Display access must use mutex protection
- Homing calibration must complete before any operation mode can start
- Speed knob preflight check is a safety requirement — don't bypass it
- BLE disconnect must always trigger gradual ramp-down, never instant stop
- StrokeEngine keepout boundaries are safety-critical — respect them

## Workflow

- Never commit changes automatically. Let me review and commit manually.
- Be opinionated but explain reasoning.
- If you learn something new about this project, add it to this file.
- Always verify state machine transitions when modifying device behavior.
