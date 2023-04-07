# WARNING - This is not up to date based on current discussion in the KM Channel
The following items are being discussed

# T-Code Extended (TCx) - Tenants of TCx
- Simple to understand, implement, and parse
- Human-readable, *interactable*
- Case-Insensitive

TCx is an backwards-compatible extension of T-Code, meant to support interfacing.
While TCx encourages using newer syntax and styling, the older style is fully compatible.

TCx is heavily based on [RESP](https://redis.io/docs/reference/protocol-spec), the protocol behind Redis. Internally, a lot of configuration is handled in a similar way to redis, as just a large key-value store. Drawing on these similarities helps build 

TCx also borrows from Lighting Industry Protocols, specifically [ArtNet](https://www.artisticlicence.com/WebSiteMaster/User%20Guides/art-net.pdf) which uses TimeCodes to maintain precise syncronization between all devices and media being played.

TCx Parameters are simple a Key-Value store, similar to a CANopen Object Dictionary, or a Redis Database. These values can either be stored **in-memory**, **on-flash**, or be dynamically fetched/set via callback functions. 

- **in-memory** values are ephemeral, and will be lost when the power is cycled
- **on-flash** values are stored via ESP32 Preferences on the SPI Flash itself, and will survive power cycling

# Handling un-reliable communication mediums
Bluetooth, WiFi and the other communication mediums TCx may be transmitted over are subject to Latecy, Packet Drops, and other issues the might prevent a TCx message being successfully delivered.

To address this, a TCx , 

# Movement
All movement within TCx is relative, and within a range of `[0-0.9999...)`. 

For Linear Axises, this relative range is mapped to a physical range using DEPTH and STROKE. 

![](https://github.com/theelims/StrokeEngine/raw/main/doc/coordinates.svg)

For Rotary Axises, this range is mapped to the `[0-360)` degrees of movement the axis has.

# Commands - Parameters
### PING
### HELP
### SET
### GET

# Commands - Power
### STOP
### POWER

# Commands - Direct Control
There are four different types of Direct Control commands
- Linear
- Rotate
- Vibrate
- Auxiliary

Commands for Linear Move, Rotate, Vibrate, and Auxilliary take the form of a letter (“L”, “R”, “V”, or "A") followed by a set of digits. Lower case letters (“l”, “r”, “v”) are also valid.

Each Direct Control command starts with the Type, Channel, then a Value

`L<channel><value>`

The value corresponds to a linear position, rotational position, or vibration level. In situations where position or speed is possible in a forward or reverse direction 0.5 is assumed to be central or at rest.

# Commands - Synced Control - TimeCodes
While Direct Control works for exactly that, direct control, if TCx is being synced up to a video or other media, maintaining syncronization is crucial.

When in Synced Control mode, TimeCodes are sent periodically, and all Direct Control commands must be prefixed with a `T<time>` code which allows the reciever to know when the command should be run.

When in this mode, it is recommended to send commands in advance of when they appear in the media. If a suitable buffer of atleast 250ms is maintained, smooth playback should be achieved.

# Commands - Engine Control - Automatic / Patterns
Engine Control allows the device itself to control the Axis based on internally generated motion targets. The most basic example of this would a Stroke Engine, which allows setting a Stroke/Pattern to be used. Once started, the Axis will run that pattern repeatedly, without any user input required.

When an Axis is under the control of an engine, all Direct/Synced Control codes will be rejected with an Error Response.

# Responses
Every command executed will recieve a Response. This response can be of several different types, and the type is determined by the first character of the response
## Strings
This type is 
## Errors

## Integers
Integers are prefixed with `:` byte.
Integers can have any number of digits returned, but will never exceed the range of a signed 64-bit Integer. A `-` byte represents negative Integers

# Modes
Modes allow changing how a specific axis is driven. There are two basic modes for an Axis to be in
- Direct
- Sync
- Engine

Direct and Sync modes correspond to the Direct Control and Synced Control described above. Engine control however, enables guided automatic control of that axis. Direct/Sync commands will not be accepted when an Axis is in Engine mode.

```
PING
+PONG

HELP
+HELP
+HELP <PARAMETER>
+SET <PARAMETER> <VALUE>
+GET <PARAMETER>
+STOP
+POWER [ON|OFF]
+<AXIS> <VALUE>
+<AXIS> <VALUE> S<SPEED>
+<AXIS> <VALUE> I<INTERVAL>
+<AXIS> S<SPEED>

MODE L0 DIRECT
L00250
+Ok
L0 0250
+Ok
L0 0250 S0020
+Ok

MODE L0 SYNC
TIMESYNC 0000
T100 L0 250 S200
T200 L0 500 S300
TIMESYNC 0230
T300 L0 750 S100

STOP
+Ok
STOP L0
+Ok
POWER OFF
+Ok

HELP L0.LENGTH
+
SET L0.LENGTH 200MM
+Ok
SET L0.DEPTH 20MM
+Ok
SET L0.STROKE 200MM
+Ok
SET L0.STORKE 200MM
+Unknown Parameter "STORKE"
SET L0.STROKE 2IN
+Ok
SET L0.STROKE 20mPa
+Unknown Unit "mPa" for Parameter "STROKE"
+Acceptable units are "IN", "MM"

SET L0.STROKE 200MM
+Ok
SET L0.DEPTH 20MM
+Ok

GET L0.STROKE
:200MM

GET L0.DEPTH
:20MM


GET VERSION.PROTOCOL
+TC++ 0.1
GET VERSION.DEVICE
+OSSM
GET VERSION.FIRMWARE
+0.24

GET PATTERNS
*4
:0
+Simple Stroke
:1
+Teasing
:2
+Robo Stroke
:3
+Half'n'Half


```