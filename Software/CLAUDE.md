# CLAUDE.md - OSSM Firmware

## Overview

OSSM (Open Source Sex Machine) is an ESP32-based stepper motor device firmware. It drives a linear actuator with pattern-based motion control, BLE remote control, an OLED display, and rotary encoder UI. Built with PlatformIO, Arduino framework, and Boost.SML state machine. Communicates with the RAD Dashboard via BLE and WiFi.

## Tech Stack

- **MCU:** ESP32 (esp32dev, 16MB flash, platform espressif32@6.12.0)
- **Framework:** Arduino + ESP-IDF (`framework = arduino, espidf`) + FreeRTOS
- **C++ Standard:** C++17 (gnu++17)
- **State Machine:** Boost.SML (header-only, `include/boost/sml.hpp`)
- **Motor:** FastAccelStepper (stepper motor with trapezoidal acceleration)
- **Display:** U8g2 (128x64 SSD1306 OLED)
- **BLE:** NimBLE-Arduino 2.4.0 + `rad-ble` (universal RAD BLE service)
- **Network:** WiFiManager portal, MQTT (RAD Dashboard), HTTPS OTA updates
- **LED:** FastLED (WS2812B status indicator)
- **Build:** PlatformIO with 5 environments (development, staging, production, hw_test, test)

## Quick Commands

```bash
pio run -e development            # Build development
pio run -e development -t upload  # Build + upload
pio run -e development -t monitor # Serial monitor (115200 baud)
pio test -e test                  # Run unit tests (native platform)
pio check                         # Run clang-tidy static analysis
```

Use PlatformIO Core 6.2+ (`~/.platformio/penv/bin/pio`, the copy the IDE installs). Older standalone installs (e.g. 6.1.x via pip) pick up the host `cc` for the ESP-IDF CMake step and fail with `unknown argument '-mlongcalls'`. Agents: pass an explicit working directory to shell commands, and run `pio` outside the sandbox (it writes to `~/.platformio`).

## Project Structure

```
Software/
├── platformio.ini               # Build config (all environments)
├── pre_build_script.py          # Build-time version injection
├── firmware_build_metadata.py   # Firmware artifact/provenance metadata
├── min_spiffs.csv               # Partition table
├── docs/
│   └── settings.md              # Persistent settings, serial console, BLE catalog
├── src/
│   ├── main.cpp                 # Entry point, FreeRTOS task creation
│   ├── stub.cpp                 # Native test-build stub (lib dependency discovery)
│   ├── ossm/
│   │   ├── state/               # Boost.SML state machine + global state
│   │   │   ├── machine.h        # Transition table (source of truth for behavior)
│   │   │   ├── state.h/cpp      # State machine instance, mutex, logger
│   │   │   ├── actions.h/cpp    # Transition actions
│   │   │   ├── guards.h/cpp     # Transition guards
│   │   │   └── settings, session, calibration, motion, menu, ble, network, error (.h/.cpp)
│   │   │                        # Global state singletons
│   │   ├── Events.h             # State machine events
│   │   ├── OSSM.h/cpp           # Deprecated compatibility shim (BLE command handling, state JSON)
│   │   ├── state_json.hpp       # State JSON serialization
│   │   ├── homing/              # Home switch calibration
│   │   ├── simple_penetration/  # Linear back-and-forth mode
│   │   ├── stroke_engine/       # Pattern-based motion mode
│   │   ├── streaming/           # Real-time BLE position streaming mode
│   │   ├── play_controls/       # STROKE→DEPTH→SENSATION cycling UI
│   │   ├── pattern_controls/    # Pattern selection UI
│   │   ├── menu/                # Menu navigation
│   │   └── pages/               # Screens: hello, preflight, help, error, wifi, pairing, update
│   ├── services/                # Hardware abstraction
│   │   ├── board.h/cpp          # Board init (GPIO, stepper, encoder, LED)
│   │   ├── stepper.h/cpp        # FastAccelStepper motor init
│   │   ├── display.h/cpp        # OLED display (mutex-protected)
│   │   ├── encoder.h/cpp        # Rotary encoder (interrupt-driven)
│   │   ├── led.h/cpp            # FastLED status indicators
│   │   ├── tasks.h/cpp          # FreeRTOS task handles, core assignments
│   │   ├── device_config.h/cpp  # NVS-persisted settings (see Persistent Settings)
│   │   ├── serial_console.h/cpp # USB settings console (115200 baud)
│   │   ├── wm.h/cpp             # WiFiManager
│   │   └── communication/
│   │       ├── nimble.h/cpp     # BLE server, characteristic UUIDs
│   │       ├── command, state, patterns, gpio, config, wifi, pairing, rename (.hpp)
│   │       │                    # Per-characteristic BLE handlers
│   │       ├── rad_ble.h/cpp    # RAD Dashboard BLE integration
│   │       ├── mqtt.h/cpp       # MQTT client
│   │       └── queue.h/cpp      # Streaming position queue
│   ├── command/                 # BLE command parsing (commands.hpp, command_pattern.hpp)
│   ├── components/              # Reusable UI components (HeaderBar)
│   ├── constants/               # Pins, Config, Menu, UserConfig, Version, Images, LogTags
│   ├── structs/                 # SettingPercents, Points, Version
│   └── utils/                   # Mutex, StateLogger, analog smoothing, OTA update,
│                                # HTTPS/TLS client, firmware MD5, BLE helpers
├── lib/                         # Hardware-independent libraries (unit-testable on native)
│   ├── StrokeEngine/            # Motion engine + patterns (StrokeEngine, pattern.h, PatternMath.h)
│   ├── OSSMLogic/               # Pure logic: guards, homing, simple penetration, streaming,
│   │                            # settings catalog + validation (settings_catalog.h, device_config_logic.h)
│   ├── FirmwareUpdateProtocol/  # OTA protocol, runtime, provenance
│   └── ui/                      # Display drawing, strings, menus, logos, text pages
├── include/
│   └── boost/sml.hpp            # State machine library
└── test/
    ├── test_*/                  # Native unit tests (Unity + ArduinoFake)
    └── test_hw_*/               # On-device hardware tests (hw_test env)
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

The transition table lives in `src/ossm/state/machine.h`. States flow through:

```
idle → homing.forward → homing.backward → menu.idle → operation mode / support state
                                       ↘ error (stroke too short / homing error)
```

- **Operation Modes:** `simplePenetration`, `strokeEngine`, `streaming`
  - Entry: not homed → `homing`; knob safe → `.idle` (running); otherwise → `.preflight` until knob reaches zero
  - `strokeEngine` adds `.pattern` (double press toggles pattern selection)
  - Exit: long press / `EmergencyStop` → `emergencyStop` + `setNotHomed` → `menu`; `ReturnToMenu` → `emergencyStop` → `menu` (stays homed); `Home` → re-home
- **Support States:** `pairing` (`.idle`, `.wifi`, `.success`, `.failed`), `update` (`.checking`, `.available`, `.installing`, `.idle`, `.failed`), `wifi`, `help`, `error` (`.idle` → `.help` → restart), `restart`
- **Homing selection:** if a mode was chosen before homing finished, `homing.backward` goes straight into that mode; first boot goes to `menu`
- **Remote menu actions:** BLE-triggered pairing/update while offline fail with a network error instead of opening the WiFi portal (`isRemoteMenuAction`)
- **AJ hardware:** `AJ_DEVELOPMENT_HARDWARE` skips homing (`idle` → `menu`)
- **Thread Safety:** `sml::thread_safe<ESP32RecursiveMutex>`
- **Logging:** `StateLogger` traces all events, guards, actions, and state changes
- Guards (`src/ossm/state/guards.*`, pure logic in `lib/OSSMLogic`) validate homing, stroke length, preflight knob position, and network availability

### FreeRTOS Tasks

Cores come from `src/services/tasks.h`: `operationTaskCore = 0`, `stepperCore = 1`. `MAX` = `configMAX_PRIORITIES - 1`.

| Task | Priority | Core | Purpose |
|------|----------|------|---------|
| `buttonTask` (`main.cpp`) | MAX | 0 | Encoder button `tick()` every 25ms |
| `initNimbleTask` (`main.cpp`) | MAX | 0 | Starts BLE + WiFiManager immediately; waits for boot homing to finish (≤120s) before MQTT and pairing status check; deletes itself |
| `nimbleLoop` (`nimble.cpp`) | MAX | 1 | BLE server loop |
| Homing / SimplePenetration / StrokeEngine / Streaming | MAX | 0 | One motion task per active mode, created on state entry |
| `publishStateTask` (`stroke_engine.cpp`) | idle+1 | 0 | StrokeEngine state publishing |
| `headerBar` (`HeaderBar.cpp`) | idle+1 | 0 | Status bar updates |
| Draw tasks (hello, menu, play/pattern controls, preflight, wifi) | 1 | any | Screen rendering, run while their state is active |
| `serialConsole` (`serial_console.cpp`) | 1 | 0 | USB settings console; started from `setup()` after the state machine |
| Update / install / pairing tasks | 1 | 0 | Network work (TLS); pause MQTT while running |

Rule: MQTT's TLS session competes with homing for internal RAM. Re-homes pause and resume MQTT themselves; preserve that when touching homing or network startup.

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

### Related Repositories

**RADR wireless remote** — [`researchanddesire/radr-wireless-remote`](https://github.com/researchanddesire/radr-wireless-remote). ESP32 handheld that controls the OSSM as a BLE client. Local clone location varies by developer (sometimes cloned as `rad-remote`); paths below are relative to that repo's root.

- OSSM client: `Software/src/devices/researchAndDesire/ossm/ossm_device.hpp` (UUIDs, commands, state parsing)
- BLE transport: `Software/src/services/coms.cpp`
- Hardware-in-the-loop scripts: `scripts/hil/`
- Read it by absolute path; don't add it to this workspace, since a second PlatformIO project confuses the tooling.
- UUIDs in its `ossm/README.md` are stale. `ossm_device.hpp` and `src/services/communication/nimble.h` here are authoritative.
- Protocol/communication changes (UUIDs, command strings, state JSON, pairing) must be mirrored in the remote, using matching branch names and linked PRs. Other changes don't need a matching branch.
- The remote has its own branch policy (feature work from `origin/staging`); see its `AGENTS.md`.
- Persistent settings, ranges, units, and how a remote should read them: `docs/settings.md`. The OSSM already accepts every setting over RAD BLE (`catalog.read` + `setting.write` after `control.acquire`). The remote has no settings UI yet; a later change should build that list from the catalog, not a hard-coded table. Do not write the legacy `speedKnobLimit` characteristic if the goal is to persist the value.

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

### Persistent Settings (NVS)

All persistence uses ESP32 `Preferences`. `device_config::load()` runs in `setup()` right after `initBoard()`, before anything converts mm to steps or reads the device name. It writes the default for any missing `ossm-cfg` key (fresh device, or a firmware that adds a key), so later boots do no writes. Once written, a key keeps its value even if a later firmware changes the default. Invalid stored geometry is ignored but not overwritten.

| Namespace | Owner | Contents |
|-----------|-------|----------|
| `ossm-cfg` | `src/services/device_config.*` | Machine geometry and live settings below |
| `radble` / `deviceName` | `rad-ble` library | Device name (source of truth) |
| `wifi` | `services/wm.cpp` | WiFi credentials |
| `fw-prov` | `FirmwareProvenance.h` | Firmware provenance tokens |

`UserConfig` / `DeviceName` is the pre-rad-ble name key; `load()` migrates it into `radble` once and deletes it.

- **Live settings** (apply immediately; globals in `UserConfig::*` and `board.h`): `displayMetric`, `afterHomingPosition`, `speedKnobAsLimit`, `latencyCompensation`, `mqttPublishFrequencyHz`. Change them only through `device_config::set*()`, which validates, skips unchanged values, and saves. `afterHomingPosition` is a fraction of the measured stroke (0 = home, 1 = full stroke), not mm.
- **Machine geometry** (`MachineConfig`: steps/rev, pulley teeth, belt pitch, max RPM, max stroke mm): read once at boot into `device_config::active()`. `saveMachineConfig()` validates and saves to `stored()`; it applies after restart. Never apply geometry at runtime — calibration and the stepper position counter are in steps.
- **Never persist motion values** (speed, stroke, depth, sensation, buffer, pattern, `speedBLE`) — the machine must boot at zero speed and pass preflight.
- **Settings catalog** (`settings_catalog.h`, documented in `docs/settings.md`): ids, units, ranges, defaults and apply rules for every persisted setting. BLE catalog strings and the USB console (`help` / `info`) read this table. Ranges are firmware facts, not NVS values.
- **USB console:** 115200 baud, started from `setup()` after the state machine. Commands: `help`, `get [id]`, `info [id]`, `set <id> <value>`, `name <value>`, `restart`, or a raw RAD BLE JSON object. Hardware tests that supply their own `setup()` do not start it.
- **Geometry validation** (`device_config_logic.h`, tested in `test/test_device_config`): all fields positive, `stepsPerRev` and `pulleyToothCount` whole numbers, max stroke > 50 mm, steps/mm ≥ 2, `maxRpm × stepsPerRev / 60 ≤ 100000` steps/s, and every derived step quantity (stroke, max acceleration) must fit in `int32`. No fixed upper limit on steps/mm. An invalid stored combination falls back to defaults at boot.
- **Step rate cap:** FastAccelStepper ignores (doesn't clamp) speeds above its limit. Use `Config::Driver::stepRateHz(mmPerSecond)` for fixed mm/s speeds; it clamps to the machine limit and 100 kHz.
- `Config::Driver::stepsPerMM()`, `maxSpeedMmPerSecond()`, `maxStrokeSteps()` and the `_mm` literal are runtime values, not `constexpr`. Don't use `_mm` in static initializers (they run before `load()`).
- BLE: RAD BLE `setting.*` paths are flagged persistent; geometry is `setting.machine.{stepsPerRev,pulleyToothCount,beltPitchMm,maxRpm,maxStrokeMm}`, and reads return `value` (stored), `active`, and `restartRequired`. NVS keys are ≤15 characters — renaming one loses the stored value.
- The legacy speed-knob-limit characteristic is session-only. RAD BLE / serial `setting.speedKnobAsLimit` is what persists. The remote still writes the legacy characteristic `false` on connect.

### Safety Features

- **Preflight check:** Speed knob must be at zero before operation starts
- **Emergency stop:** Long button press kills motor immediately
- **BLE disconnect:** Speed ramps down over 2s (not instant stop)
- **Keepout boundaries:** Soft endstops prevent travel beyond safe range
- **Home switch:** Calibration required before any operation mode

## Build Configuration

### Environments

All firmware envs extend `[common]` (shared `lib_deps`, partitions, clang-tidy config) and use `espressif32@6.12.0`, `esp32dev`, `arduino, espidf`. Default env is `development`.

| Env | Debug | Track / server | Notable Flags |
|-----|-------|----------------|---------------|
| development | 4 | local (`localhost:3000`, `mqtt://localhost`) | `VERSIONDEV`, `PRETEND_TO_BE_FLESHY_THRUST_SYNC`; uploads + monitors by default |
| staging | 3 | `staging` → staging.researchanddesire.com | `FIRMWARE_TRACK="staging"`, MQTT over TLS |
| production | 2 | `main` → dashboard.researchanddesire.com | `FIRMWARE_TRACK="main"`, MQTT over TLS |
| hw_test | 4 | extends `development` | `test_filter = test_hw_*`, `test_build_src = true` |
| test | 0 | native@1.2.1 | `VERSIONTEST`, `UNITY_INCLUDE_DOUBLE`, ArduinoFake; builds only `stub.cpp` + `lib/`; ignores `test_hw_*`, `test_measurements`, `test_display` |

`AJ_DEVELOPMENT_HARDWARE` is still referenced in code but has no env.

Only staging and production define `FIRMWARE_TRACK`; `src/constants/Version.h` defaults it to `"main"`. Include that header wherever you use the macro. Don't add other code near the `VERSION`/`*_VERSION` lines — `scripts/release_version.py` rewrites them by regex.

### Build Scripts

Both run as `pre:` extra scripts:
- `pre_build_script.py` sets `SW_VERSION` (build flag → `SW_VERSION` env var → `GITHUB_ENV`). For `VERSIONDEV` builds it rewrites `localhost` in build flags to this machine's LAN IP so the device can reach a local RAD server.
- `firmware_build_metadata.py` defines `FIRMWARE_BUILD_SHA` from `RELEASE_BUILD_SHA` or `GITHUB_SHA`.

### Flash Partitions

Uses `min_spiffs.csv` (in this directory), 16MB flash.

## Libraries

All versions are pinned exactly in `platformio.ini`.

| Library | Version | Purpose |
|---------|---------|---------|
| rad-ble (git) | pinned commit | Universal RAD BLE service |
| NimBLE-Arduino | 2.4.0 | BLE stack |
| FastAccelStepper | 0.30.15 | Stepper motor control |
| U8g2 | 2.36.18 | OLED display (SSD1306) |
| FastLED | 3.10.3 | RGB LED status |
| ArduinoJson | 7.4.3 | JSON parsing |
| QRCode | 0.0.1 | QR code generation |
| Ai Esp32 Rotary Encoder | 1.7.0 | Rotary encoder |
| OneButton | 2.6.2 | Button debouncing |
| WiFiManager (git) | pinned commit | WiFi config portal |
| ArduinoFake | 0.4.0 | Test mocking (test only) |

## Testing

### Unit tests (native, no hardware)

- Framework: Unity (PlatformIO native)
- Mocking: ArduinoFake
- Run: `pio test -e test`

### Hardware tests (on-device, USB-connected OSSM)

- Environment: `hw_test` (extends `development`, builds full `src/`)
- Run all: `pio test -e hw_test`
- Run one: `pio test -e hw_test -f test_hw_smoke`
- Available suites: `test_hw_smoke`, `test_hw_homing`, `test_hw_homing_error`, `test_hw_pairing`, `test_hw_regressions`, `test_hw_state_machine`, `test_hw_wifi`
- `test_hw_homing` moves the motor — keep the rail clear
- Hardware tests that call `initBoard()` must call `device_config::load()` right after it, same as `main.cpp`, so they use the geometry stored in NVS

## Important Rules

- Never commit credentials or WiFi passwords
- State machine (`src/ossm/state/machine.h`) is the single source of truth for device behavior
- Display access must use mutex protection
- Homing calibration must complete before any operation mode can start
- Speed knob preflight check is a safety requirement — don't bypass it
- BLE disconnect must always trigger gradual ramp-down, never instant stop
- StrokeEngine keepout boundaries are safety-critical — respect them (`Config::Driver::keepoutBoundaryMm` is shared by StrokeEngine and the stepper frame translation)
- Machine geometry from NVS applies only at boot — never change steps/mm while running

## Workflow

- Never commit changes automatically. Let me review and commit manually.
- Before making any code edit, explicitly state what the change does, what the side effects are, and what areas are affected. Wait for explicit approval before editing any file.
- Be opinionated but explain reasoning.
- If you learn something new about this project, add it to this file.
- Always verify state machine transitions when modifying device behavior.
