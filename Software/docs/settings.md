# OSSM persistent settings

Firmware source of truth: `lib/OSSMLogic/src/settings_catalog.h`.
Validation: `lib/OSSMLogic/src/device_config_logic.h`.
BLE catalog strings and the USB console both read the catalog, so ranges
and units stay in one place. They are **not** stored in NVS — a later
firmware that changed a limit would otherwise keep advertising the old one.

Motion values (speed, stroke, depth, sensation, buffer, pattern, `speedBLE`)
are never persisted. The machine must boot at zero speed and pass preflight.

## USB serial console

115200 baud, LF line endings. `pio device monitor` echoes locally
(`monitor_echo = yes`). Hardware tests do not start the console.

```text
help
get                     # every setting, plus units and range
get steps_per_rev       # one setting (geometry includes stored/active)
info home_position      # description, range, apply rule
set home_position 0.5
set metric false
set max_stroke 300      # saved now; used after restart
name "Kitchen OSSM"     # saved now; BLE advertisement after restart
restart                 # blocked during homing or a motion mode
{"op":"setting.read","path":"setting.machine.maxRpm"}
```

Replies start with `ok` or `err <code>: <message>`. Serial writes skip the
BLE control lease (physical USB access). Logs share the port, so a reply
can sit next to an ESP_LOG line.

## Settings

| Serial id | BLE path | Type | Units | Range | Default | Applies |
|-----------|----------|------|-------|-------|---------|---------|
| `speed_limit` | `setting.speedKnobAsLimit` | bool | — | true\|false | true | immediately |
| `latency` | `setting.latencyCompensation` | bool | — | true\|false | false | immediately |
| `metric` | `setting.displayMetric` | bool | — | true\|false | true | immediately |
| `home_position` | `setting.afterHomingPosition` | float | fraction of stroke | 0..1 | 0 | next homing |
| `mqtt_rate` | `setting.mqttPublishFrequency` | float | Hz | 1..100 | 30 | immediately |
| `steps_per_rev` | `setting.machine.stepsPerRev` | int | steps | > 0, whole number | 800 | after restart |
| `pulley_teeth` | `setting.machine.pulleyToothCount` | int | teeth | > 0, whole number | 20 | after restart |
| `belt_pitch` | `setting.machine.beltPitchMm` | float | mm | > 0 | 2 | after restart |
| `max_rpm` | `setting.machine.maxRpm` | float | rpm | > 0 | 1500 | after restart |
| `max_stroke` | `setting.machine.maxStrokeMm` | float | mm | > 50 | 500 | after restart |
| `device_name` | `device.name` | string | — | 1..24 bytes, `A-Z a-z 0-9 space . - _` | OSSM | name saved now; advertisement after restart (live if written through RAD BLE) |

`home_position` is a fraction, not millimetres: 0 is the home end, 1 is full stroke.

### Geometry combination rules

These are checked on the whole `MachineConfig`, not on one field:

- `steps/mm = steps_per_rev / (pulley_teeth * belt_pitch)` must be at least 2
- `max_rpm * steps_per_rev / 60` must be at most 100000 steps/s
- `steps/mm * max_stroke` and `steps/mm * 50000` (max acceleration mm/s²) must fit in a 32-bit integer (steps/mm max about 42949)

A rejected combination is not saved. Invalid geometry already in NVS is
ignored at boot and the defaults are used; the stored keys are not overwritten.

## BLE (RAD BLE v1)

Service UUID: `522b443a-4f53-534d-0001-420badbabe69`.

The command characteristic (`…-1000-…`) accepts both legacy text
(`go:`, `set:`, `stream:`) and JSON RAD requests. JSON replies are
**indications** on the response characteristic (`…-1100-…`). Subscribe
with `subscribe(false, callback)`, not notify (`true`).

Read units and ranges from the device; do not hard-code them:

```json
{"v":1,"id":1,"op":"catalog.read","args":{"page":0,"pageSize":32}}
```

Each catalog entry has `path`, `valueType`, `units`, `constraints`
(`min` / `max` / `exclusiveMin` / `integer` / `restartRequired` /
`maxBytes`), plus `leaseRequired` and `safetyCritical`.

Write a setting:

```json
{"v":1,"id":2,"op":"control.acquire","args":{"ttl":10}}
{"v":1,"id":3,"op":"setting.write","path":"setting.machine.maxStrokeMm","args":{"value":300},"lease":<token>}
{"v":1,"id":4,"op":"setting.read","path":"setting.machine.maxStrokeMm","lease":<token>}
```

Geometry reads return `value` (stored), `active` (in use since boot), and
`restartRequired`. Device name (`device.name`) is handled by rad-ble
before the OSSM command handler: it allows 24 bytes, letters/numbers/space
`. - _`, and an empty value resets to `OSSM`.

`setting.write` on a persistent path saves to NVS. Safety-critical
geometry paths are flagged `RWSP`.

### Legacy characteristics the remote still uses

| Name | UUID suffix | Persist? |
|------|-------------|----------|
| Command (text + JSON) | `1000` | JSON `setting.write` yes; `go:`/`set:` motion no |
| Speed knob as limit | `1010` (`ossm_device.hpp` `speedKnobLimit`) | **No** — session only |
| Latency compensation | see `nimble.h` | Yes |
| Rename | see `nimble.h` | Yes, then restart |

The remote currently writes `speedKnobLimit=false` on every connect
(`ossm_device.hpp`). That must stay session-only so it does not overwrite
the saved `speed_limit`. To persist the preference, use
`setting.write` on `setting.speedKnobAsLimit`.

## NVS

| Namespace | Key examples | Owner |
|-----------|--------------|-------|
| `ossm-cfg` | `stepsPerRev`, `pulleyTeeth`, `beltPitchMm`, `maxRpm`, `maxStrokeMm`, `metric`, `homePosition`, `knobLimit`, `latencyComp`, `mqttHz` | `device_config` |
| `radble` | `deviceName` | rad-ble |

`device_config::load()` writes the catalog default for any missing
`ossm-cfg` key. After that, a stored value wins even if a later firmware
changes the default.

## RADR remote (later change)

Do not add the remote repo to this workspace. Matching branches are only
required for protocol/UUID/command-string changes. This settings work does
**not** change UUIDs or command strings, so a settings UI can land on its
own remote branch.

Suggested approach for a later agent:

1. Read `Software/docs/settings.md` here and `settings_catalog.h` if a
   range is unclear. Catalog JSON from the device is authoritative at
   runtime.
2. In `radr-wireless-remote`, the OSSM client is
   `Software/src/devices/researchAndDesire/ossm/ossm_device.hpp`.
   Transport is `Software/src/services/coms.cpp`.
3. Add the response characteristic
   `522b443a-4f53-534d-1100-420badbabe69` and subscribe for indications.
4. Acquire / renew / release the control lease before `setting.write`.
5. Build the settings list from `catalog.read` (filter `category=setting`
   and persistent flags). Show `units` and `constraints`. Do not clone
   the table above into firmware constants on the remote.
6. Stop relying on the legacy `speedKnobLimit` write if the UI can use
   `setting.speedKnobAsLimit`. Until then, leaving the session-only write
   is safe.
7. `ossm/README.md` UUIDs in the remote repo are stale; ignore them.
8. Remote branch policy: feature work from `origin/staging` (see that
   repo's `AGENTS.md`).
