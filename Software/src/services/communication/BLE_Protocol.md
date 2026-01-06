# OSSM BLE Protocol Documentation

## Overview

The OSSM (Open Source Sex Machine) uses Bluetooth Low Energy (BLE) for wireless control and monitoring. This document describes the complete BLE protocol implementation for technical users developing client applications.

## Service Architecture

### Primary Service UUID

```
522b443a-4f53-534d-0001-420badbabe69
```

The OSSM implements a custom BLE service with multiple characteristics organized into functional groups based on a [structured namespace](#namespace-ranges)

## Characteristic Specifications

### Command Characteristics (Writable)

#### Primary Command Characteristic

-   **UUID**: `522b443a-4f53-534d-1000-420badbabe69`
-   **Properties**: READ, WRITE
-   **Purpose**: Send commands to control OSSM behavior

**Command Format**:

```
set:<parameter>:<value>
go:<state>
```

**Available Commands**:

| Command                 | Parameter | Value Range | Description                                                         |
| ----------------------- | --------- | ----------- | ------------------------------------------------------------------- |
| `set:speed:<value>`     | speed     | 0-100       | Set stroke speed percentage                                         |
| `set:stroke:<value>`    | stroke    | 0-100       | Set stroke length percentage                                        |
| `set:depth:<value>`     | depth     | 0-100       | Set penetration depth percentage                                    |
| `set:sensation:<value>` | sensation | 0-100       | Set sensation intensity percentage                                  |
| `set:pattern:<value>`   | pattern   | 0-6         | Set stroke pattern (see patterns list)                              |
| `go:simplePenetration`  | -         | -           | Switch to simple penetration mode from the menu                     |
| `go:strokeEngine`       | -         | -           | Switch to stroke engine mode from the menu                          |
| `go:menu`               | -         | -           | Return to main menu from either stroke engine or simple penetration |

**Response Format**:

-   **Success**: `ok:<original_command>`
-   **Failure**: `fail:<original_command>`

**Example Commands**:

```
set:speed:75
set:pattern:3
go:strokeEngine
```

#### Speed Knob Configuration Characteristic

-   **UUID**: `522b443a-4f53-534d-1010-420badbabe69`
-   **Properties**: READ, WRITE
-   **Purpose**: Configure whether speed knob acts as upper limit for BLE speed commands

**Configuration Values**:

| Value             | Description                              |
| ----------------- | ---------------------------------------- |
| `true`, `1`, `t`  | Speed knob acts as upper limit (default) |
| `false`, `0`, `f` | Speed knob and BLE speed are independent |

**Behavior**:

-   **When `true`**: BLE speed commands (0-100) are treated as a percentage of the current physical knob value
    -   Example: Knob at 50%, BLE command `set:speed:80` → Effective speed = 40%
-   **When `false`**: BLE speed commands (0-100) are used directly as the speed value
    -   Example: BLE command `set:speed:80` → Effective speed = 80%

**Response Format**:

-   **Success**: Current configuration value (`true` or `false`)
-   **Error**: `error:invalid_value` for invalid input

### State Characteristics (Read-Only)

#### Current State Characteristic

-   **UUID**: `522b443a-4f53-534d-2000-420badbabe69`
-   **Properties**: READ, NOTIFY
-   **Purpose**: Monitor current OSSM state and settings

**State JSON Format**:

```json
{
  "state": "<state_name>",
  "speed": <0-100>,
  "stroke": <0-100>,
  "sensation": <0-100>,
  "depth": <0-100>,
  "pattern": <0-6>
}
```

**Available States**:

See the entire list here: [OSSM State Machine](../src/ossm/OSSM.h)

-   `idle` - Initializing
-   `homing` - Homing sequence active
-   `homing.forward` - Forward homing in progress
-   `homing.backward` - Backward homing in progress
-   `menu` - Main menu displayed
-   `menu.idle` - Menu idle state
-   `simplePenetration` - Simple penetration mode
-   `simplePenetration.idle` - Simple penetration idle
-   `simplePenetration.preflight` - Pre-flight checks
-   `strokeEngine` - Stroke engine mode
-   `strokeEngine.idle` - Stroke engine idle
-   `strokeEngine.preflight` - Pre-flight checks
-   `strokeEngine.pattern` - Pattern selection
-   `update` - Update mode
-   `update.checking` - Checking for updates
-   `update.updating` - Update in progress
-   `update.idle` - Update idle
-   `wifi` - WiFi setup mode
-   `wifi.idle` - WiFi setup idle
-   `help` - Help screen
-   `help.idle` - Help idle
-   `error` - Error state
-   `error.idle` - Error idle
-   `error.help` - Error help
-   `restart` - Restart state

**Notification Behavior**:

-   State changes trigger immediate notifications
-   Periodic notifications every 1000ms if no state change
-   Notifications stop when no clients connected

### Pattern Information Characteristics

#### Pattern List Characteristic

-   **UUID**: `522b443a-4f53-534d-3000-420badbabe69`
-   **Properties**: READ
-   **Purpose**: Get available stroke patterns

**Pattern JSON Format**:

```json
[
    {
        "name": "Simple Stroke",
        "idx": 0
    },
    {
        "name": "Teasing Pounding",
        "idx": 1
    },
    {
        "name": "Robo Stroke",
        "idx": 2
    },
    {
        "name": "Half'n'Half",
        "idx": 3
    },
    {
        "name": "Deeper",
        "idx": 4
    },
    {
        "name": "Stop'n'Go",
        "idx": 5
    },
    {
        "name": "Insist",
        "idx": 6
    }
]
```

#### Pattern Description Characteristic

-   **UUID**: `522b443a-4f53-534d-3010-420badbabe69`
-   **Properties**: READ, WRITE
-   **Purpose**: Get the description for a stroke pattern

Write the pattern index to the characteristic to obtain the pattern description

For example to read the description of the pattern with an index of 0:
1. Write `0` to the characteristic
2. Read "Acceleration, coasting, deceleration equally split; no sensation." from the characteristic


**Pattern Descriptions**:

-   **Simple Stroke (0)**: Acceleration, coasting, deceleration equally split; no sensation
-   **Teasing Pounding (1)**: Speed shifts with sensation; balances faster strokes
-   **Robo Stroke (2)**: Sensation varies acceleration; from robotic to gradual
-   **Half'n'Half (3)**: Full and half depth strokes alternate; sensation affects speed
-   **Deeper (4)**: Stroke depth increases per cycle; sensation sets count
-   **Stop'n'Go (5)**: Pauses between strokes; sensation adjusts length
-   **Insist (6)**: Modifies length, maintains speed; sensation influences direction

## Device Information Service

The OSSM also implements the standard Device Information Service (UUID: `180A`) with:

-   **Manufacturer Name** (UUID: `2A29`): "Research And Desire"
-   **System ID** (UUID: `2A23`): `88:1A:14:FF:FE:34:29:63`

## UUID Namespace Structure

The OSSM uses a structured UUID namespace for easy expansion:

### Service UUID

```
0x0001 = Service UUID
```

### Namespace Ranges

| Range | Hex Range     | Description                         |
| ----- | ------------- | ----------------------------------- |
| 0x0   | 0x0000–0x0FFF | Reserved for future System Messages |
| 0x1   | 0x1000–0x1FFF | Commands and Configuration          |
| 0x2   | 0x2000–0x2FFF | State Information                   |
| 0x3   | 0x3000–0x3FFF | Pattern Information                 |
| 0x4   | 0x4000–0x4FFF | GPIO Pin Setting                    |
| 0x5   | 0x5000–0x5FFF | Future                              |
| 0x6   | 0x6000–0x6FFF | Future                              |
| 0x7   | 0x7000–0x7FFF | Future                              |
| 0x8   | 0x8000–0x8FFF | Future                              |
| 0x9   | 0x9000–0x9FFF | Future                              |
| 0xA   | 0xA000–0xAFFF | Future                              |
| 0xB   | 0xB000–0xBFFF | Future                              |
| 0xC   | 0xC000–0xCFFF | Future                              |
| 0xD   | 0xD000–0xDFFF | Future                              |
| 0xE   | 0xE000–0xEFFF | Reserved For Statistics             |
| 0xF   | 0xF000–0xFFFF | Experimental / Sandbox - Volatile   |

### Current Characteristic Assignments

#### Commands and Configuration (0x1000-0x1FFF)

```
522b443a-4f53-534d-1000-420badbabe69  # Primary command
522b443a-4f53-534d-1010-420badbabe69  # Speed knob configuration
```

#### State Information (0x2000-0x2FFF)

```
522b443a-4f53-534d-2000-420badbabe69  # Current state
```

#### Pattern Information (0x3000–0x3FFF)

```
522b443a-4f53-534d-3000-420badbabe69  # Pattern list
522b443a-4f53-534d-3010-420badbabe69  # Pattern description
```

## Connection Management

### Advertising

-   **Device Name**: "OSSM"
-   **Service UUIDs**: Primary service + Device Information Service
-   **Advertising Interval**: 20-40ms (optimized for reliability)
-   **Auto-restart**: Advertising resumes when all clients disconnect

### Security

-   **Pairing**: "Just Works" pairing (no authentication required)
-   **Encryption**: BLE Secure Connections enabled
-   **Bonding**: Disabled (no persistent pairing)

## Implementation Notes

### Command Processing

-   Commands are validated using regex patterns
-   Invalid commands return `fail:` response
-   Valid commands are processed by the state machine
-   Command processing is non-blocking

### State Monitoring

-   State updates are sent via notifications
-   Clients should subscribe to notifications for real-time updates
-   State changes are logged at DEBUG level

### Error Handling

-   Invalid commands are logged and rejected
-   Connection errors are handled gracefully
-   Device automatically restarts advertising on disconnect

## Client Implementation Guidelines

### Connection Flow

1. Scan for "OSSM" device name
2. Connect to device
3. Discover services and characteristics
4. Subscribe to state notifications
5. Read initial state and pattern list
6. Send commands as needed

### Command Best Practices

-   Always validate command format before sending
-   Handle both `ok:` and `fail:` responses
-   Implement retry logic for critical commands
-   Monitor state changes for command confirmation

### State Monitoring

-   Subscribe to state characteristic notifications
-   Parse JSON state updates
-   Handle state transitions appropriately
-   Implement timeout for missing updates

## Example Client Code

### JavaScript/Web Bluetooth

```javascript
// Connect to OSSM
const device = await navigator.bluetooth.requestDevice({
    filters: [{ name: "OSSM" }],
    optionalServices: ["522b443a-4f53-534d-0001-420badbabe69"],
});

const server = await device.gatt.connect();
const service = await server.getPrimaryService("522b443a-4f53-534d-0001-420badbabe69");

// Get characteristics
const commandChar = await service.getCharacteristic("522b443a-4f53-534d-1000-420badbabe69");
const stateChar = await service.getCharacteristic("522b443a-4f53-534d-2000-420badbabe69");
const speedKnobConfigChar = await service.getCharacteristic("522b443a-4f53-534d-1010-420badbabe69");
const patternsChar = await service.getCharacteristic("522b443a-4f53-534d-3000-420badbabe69");

// Subscribe to state updates
await stateChar.startNotifications();
stateChar.addEventListener("characteristicvaluechanged", (event) => {
    const state = JSON.parse(new TextDecoder().decode(event.target.value));
    console.log("State update:", state);
});

// Configure speed knob behavior
await speedKnobConfigChar.writeValue(new TextEncoder().encode("true")); // Knob as limit
// or
await speedKnobConfigChar.writeValue(new TextEncoder().encode("false")); // Independent

// Send command
const command = "set:speed:75";
await commandChar.writeValue(new TextEncoder().encode(command));

// Read patterns
const patterns = await patternsChar.readValue();
const patternList = JSON.parse(new TextDecoder().decode(patterns));
console.log("Available patterns:", patternList);
```

### Python (bleak)

```python
import asyncio
import json
from bleak import BleakClient

async def connect_to_ossm():
    async with BleakClient("OSSM") as client:
        # Subscribe to state updates
        await client.start_notify(
            "522b443a-4f53-534d-2000-420badbabe69",
            state_callback
        )

        # Configure speed knob behavior
        await client.write_gatt_char(
            "522b443a-4f53-534d-1010-420badbabe69",
            "true".encode()  # Knob as limit
        )

        # Send command
        command = "set:speed:75"
        await client.write_gatt_char(
            "522b443a-4f53-534d-1000-420badbabe69",
            command.encode()
        )

def state_callback(sender, data):
    state = json.loads(data.decode())
    print(f"State update: {state}")

asyncio.run(connect_to_ossm())
```

## Troubleshooting

### Common Issues

-   **Connection fails**: Ensure device is in range and advertising
-   **Commands not working**: Check command format and current state
-   **No state updates**: Verify notification subscription
-   **Invalid responses**: Check command validation logic

### Debug Information

-   Enable ESP32 logging at DEBUG level for detailed protocol information
-   Monitor BLE connection status and MTU changes
-   Check command parsing and state machine transitions

---

_This documentation is maintained alongside the OSSM firmware. For updates and issues, refer to the project repository._
