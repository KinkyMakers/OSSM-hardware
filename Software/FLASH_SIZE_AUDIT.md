# Firmware flash-size audit

This hotfix keeps the current application on both 4 MB and 16 MB ESP32 boards.
Patterns, Simple Penetration, streaming, wired controls and display, homing and
motion limits, legacy BLE, RAD BLE, pairing, Wi-Fi setup, dashboard MQTT and both
OTA paths remain in scope. Removing one of those features is not a size saving
accepted by this change.

## Source and attribution

The branch was created from `origin/main` and refreshed to
`6957ecd5797c290fe25914451918b59767379b7c` (1.0.57) after OSSM PR 334 merged.
It preserves the prerequisite boot/Wi-Fi/BLE work through
`5046dd38041b631a167597620afec87c2d747a95`. RAD BLE remains pinned
to `040f37172dfb4a02e26362ada46e09504c66cb1c` (researchanddesire/rad-ble PR 1).
Their changes are included in the baseline, not counted as this cleanup's savings.

[OSSM-Lite by fray-d](https://github.com/fray-d/OSSM-Lite) was the starting point
for the unused-code audit, inspected at
`76a894f269573a42530bde8a170a0de14c2ed105`. Relevant prior work:

- [StrokeEngine cleanup, 7a9e80a](https://github.com/fray-d/OSSM-Lite/commit/7a9e80a0d45f670d3475df014a83f41cf5dc87e3).
- [Verbose state cleanup, 62e3ccb](https://github.com/fray-d/OSSM-Lite/commit/62e3ccbcc09ffe8063e09f80ff5a00c2cbcebe9d).
- [Display cleanup, 5cee14d](https://github.com/fray-d/OSSM-Lite/commit/5cee14d51eb34775a30ff53c5c7aa49795bc1ff1).
- [Orphan version-struct removal, 1dd0caa](https://github.com/fray-d/OSSM-Lite/commit/1dd0caa6983f650f4fa2efaacb3301ac5e1a1202).

Selected deletion ideas are adapted and checked against this repository's callers;
OSSM-Lite's broader feature removals and replacement OTA behavior are not imported.
The original StrokeEngine copyright notices and its [MIT license](lib/StrokeEngine/LICENSE)
remain intact. New parsers, build profiles, partition reporting and validation
plumbing are independent implementations. Relevant cleanup commits also carry
upstream attribution.

## Measurement method

Use the same pinned platform (`espressif32@6.12.0`), Arduino 2.0.17, ESP-IDF
4.4.7 and Xtensa GCC 8.4.0 throughout. Compare the actual `firmware.bin` byte
count, then use the ELF/link map to explain it. Archive sizes are not additive:
shared code and linker garbage collection can make source deletions save zero.

The fresh-main baseline (including the merged prerequisite fixes) was built with:

```sh
RELEASE_BUILD_SHA=6957ecd5797c290fe25914451918b59767379b7c \
  pio run -e production -t buildprog
```

| Measurement | Fresh-main baseline |
| --- | ---: |
| Application binary | 2,028,736 bytes |
| ELF flash usage | 2,022,153 bytes |
| Static RAM | 66,556 bytes |
| 4 MB application slot | 1,966,080 bytes |
| 4 MB target after 16 KiB margin | 1,949,696 bytes |
| Reduction required to meet that target | 79,040 bytes |

Baseline application SHA-256:
`ca375b792a86dfb43d4b418de0ac4e815c44e3dc095c3b63f8972da7b6bcb22c`.
Build timestamps mean a repeated build can have a different digest; source SHA,
toolchain, environment, size and its own artifact hash must travel together.
For the size comparisons, `RELEASE_BUILD_SHA` is held at the baseline value
while each cleanup is applied. These comparison images are not published or
flashed; hardware candidates must be rebuilt with their committed source SHA.

## Phase 1: unused code and build overhead

- Remove uncalled StrokeEngine setup/homing/telemetry APIs and unused display
  helpers; retain application homing, origin correction, clipping and patterns.
- Replace both BLE regex validators with allocation-free grammar checks. Keep
  transport validation separate from command execution and preserve GPIO's
  error order, whitespace, case handling and acknowledgements. Native tests
  compare the replacement against the previous regex grammar.
- Avoid constructing logger strings when the corresponding log is compiled out.
  Development logs and state-machine guards remain enabled.
- Use size optimization (`-Os`) for both release tracks and capacities, retaining
  assertions, watchdogs, brownout protection and existing rollback settings.
  Its stronger data-flow analysis exposed an uninitialized previous streaming
  command. Initialize that history and handle zero-distance/repeated packets
  without division by zero; retain the existing reversal and motion limits.
  IDF 4.4.7 enables its task-return diagnostic wrapper only with default
  optimization, so release `-Os` builds lose that diagnostic/GDB wrapper.
  Task exits still delete themselves or restart; the safety guards remain.
- Apply the already-declared `RADBLE_OMIT_SURFACE_CHARACTERISTICS` build flag.
  OSSM only enables sensor-stream/application-OTA channels; none of its existing
  optional surface characteristics is removed.
- Add current-source `production_4mb` and `staging_4mb` environments. Existing
  `production` and `staging` environments remain 16 MB.
- Share installed-partition detection between network update reports and RAD BLE
  identity. Validate both images, bootloader headers, exact partition tables,
  install offsets and the smallest OTA slot, including the safety margin.
- Build both hardware variants from one release version/SHA, with distinct
  immutable artifacts and validation records. CI no longer substitutes frozen
  1.0.39 source as proof that current firmware works on 4 MB.

| Phase 1 profile | Application bytes | Free space after the 16 KiB OTA margin |
| --- | ---: | ---: |
| `production` (16 MB) | 1,545,584 | 6,302,352 |
| `production_4mb` | 1,545,472 | 404,224 |
| `staging` (16 MB) | 1,553,248 | 6,294,688 |
| `staging_4mb` | 1,553,312 | 396,384 |

Production saves **483,152 bytes (23.8%)** against fresh main. Static RAM drops
from 66,556 to 59,228 bytes on all four profiles. All four builds, ESP image
checksums/headers, known partition geometries, merged-image checks and size
gates pass. All 217 configured native tests and 45 Python helper/version tests
pass; the changed workflows pass actionlint. Native display/hardware suites
remain excluded by the existing native profile. No device result is claimed.

Controlled builds apply each source group cumulatively at the fixed baseline
SHA. Completed measurements at this checkpoint:

| Change applied | Application bytes | Bytes saved by this step |
| --- | ---: | ---: |
| Fresh main | 2,028,736 | — |
| Unused StrokeEngine/display/Font/orphan code | 2,025,120 | 3,616 |
| Both regex validators replaced | 1,795,232 | 229,888 |
| Disabled logger overhead removed | 1,786,528 | 8,704 |

The remaining flag/optimization/layout measurements continue independently.
These are observed image deltas, including linker alignment and shared-code
effects. Unused-source deletion alone would still exceed the 4 MB target.

The parsers matched all **149,760** comparisons against the previous regexes;
the same suite passes AddressSanitizer and UndefinedBehaviorSanitizer. Target
Xtensa compile checks confirm disabled logging retains no String/log calls,
while development debug/verbose configurations retain diagnostics without
constructing those Strings.

## Phase 2: library consolidation

The audit identified the following bounded candidates; measured results and
validation are recorded as each change is implemented:

| Candidate | Reason | Compatibility boundary |
| --- | --- | --- |
| NimBLE TinyCrypt to mbedTLS | mbedTLS already supplies BLE primitives | Keep bonding, encryption, secure connections and peripheral behavior |
| Remove central/observer roles | No firmware BLE client or scanning use | Keep peripheral/broadcaster, connections, MTU and scan responses |
| Pairing HTTPClient to `esp_http_client` | OTA already uses the IDF client | Keep auth fields/headers, status handling, polling and task boundaries; use the existing trust bundle |
| MQTT WebSocket transports off | Only MQTT TCP/TLS is used | Preserve MQTT SSL and dashboard streaming/reconnect |
| mbedTLS client-only TLS | No TLS server exists in firmware | Retain client algorithms, full CA bundle and BLE cryptography |

## Deliberately retained

- WiFiManager and its plain-HTTP captive portal: provisioning is core behavior.
- Arduino WiFi and Preferences: these wrap existing IDF services, not second
  independent Wi-Fi or NVS stacks.
- Arduino Update: still shared by network OTA, direct BLE OTA fallback/filesystem
  paths and WiFiManager. Replacing it is a separate safety-sensitive migration.
- ArduinoJson: the baseline contains only 89 retained bytes of cJSON, not a second
  full JSON implementation worth replacing the application serializer for.
- FastLED and its existing effects: the retained footprint does not justify a
  custom RMT driver with new timing/peripheral risks near motion control.
- Full CA bundle, crypto algorithm compatibility, assertions and safety checks.
- Existing dedicated TLS/update task, MQTT pause/resume, checksum verification,
  reboot conditions and pending-image confirmation. Rollback remains disabled
  by the existing bootloader configuration; tests must not claim otherwise.

## Installed layouts

| Identity | app0 offset / size | app1 offset / size |
| --- | --- | --- |
| `ossm-ota-4mb-v1` | `0x10000 / 0x1E0000` | `0x1F0000 / 0x1E0000` |
| `ossm-ota-v1` (older alternative) | `0x10000 / 0x1F0000` | `0x200000 / 0x1F0000` |
| `ossm-ota-16mb-v1` | `0x10000 / 0x780000` | `0x790000 / 0x780000` |

The 4 MB build retains the deployed NVS (`0x9000/0x5000`), OTA data
(`0xE000/0x2000`), SPIFFS (`0x3D0000/0x20000`) and coredump
(`0x3F0000/0x10000`) locations. Unknown/missing/malformed OTA layouts report
`unknown`. OTA remains application-only where the selected release contains
only an application; it does not repartition devices. Preserve NVS during USB
validation by using explicit multipart writes, not a merged image over its gaps.

## Validation status and boundaries

Hardware validation is pending. The higher-priority task explicitly released
both ports and BLE connections after closing its loggers and clients. Its previous
frozen 4 MB firmware result is not evidence for this branch's current-source
4 MB image. Candidate builds, size checks and the visual safety gate must pass
before this branch is flashed.
The attempted fresh OSSM screen snapshot timed out waiting for the camera
stream. Until that visual gate is restored or explicitly waived for this bench,
there are no flash, reset, BLE or motion results for these candidate images.

After handoff, separately record exact board identity/capacity and artifact hash,
flash/readback/boot stability, full RADR reads versus compact subscriptions,
legacy BLE commands, pairing/security, Wi-Fi provisioning, MQTT streaming,
settings persistence and OTA success/failure behavior. Physical homing, motion
and all patterns require the bench's motion-safety and visual gates. A boot into
`error.idle` after a homing timeout is not a successful homing or motion test.

No firmware is published, promoted or declared update-eligible by this work.
Each release variant must retain its independent hardware validation gate.
