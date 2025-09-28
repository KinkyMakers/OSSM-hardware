# (WIP) RGB LED Status Indication

The RGB LED (connected to pin 25) provides visual feedback for both Bluetooth Low Energy (BLE) connection status and machine operational status.

## Status Priority

Machine operational status takes priority over BLE status:
1. **Homing** (highest priority) - Deep purple breathing effect
2. **BLE Status** (lower priority) - When machine is idle

## Machine Status Indications

### When Machine is Homing
- **Deep purple breathing effect** - Smooth breathing pattern

- Continues until homing is complete or fails

## BLE Status Indications

### When BLE Advertising (looking for connections)
1. **Rainbow effect** - 1 second
2. **Fast breathing blue** - Rapid breathing effect in blue color (10x faster), continuously while advertising
3. **Auto-dim after 30 seconds** - Dims to subtle pulsing at connection level brightness to reduce annoyance

### When BLE Connected
1. **Rainbow effect** - 1 second
2. **Fade to dim** - Blue LED fades to a low dim level (~12% brightness) and stays dimmed
3. **Communication pulses** - Subtle brightness increases during BLE communication (send/receive)

### When BLE Disconnected
- **LED Off** - No light

## Advertising Timeout Feature

To reduce visual distraction for users with wired remotes, the LED automatically dims after 30 seconds of advertising:

- **First 30 seconds**: Bright fast pulsing blue (easily visible when searching for device)
- **After 30 seconds**: Dims to connection-level brightness (~12%) with gentle pulsing
- **Purpose**: Maintains indication that BLE is available while reducing annoyance
- **Reset**: Returns to bright pulsing if BLE connection status changes

## Communication Indication

When BLE is connected and dimmed, the LED will briefly pulse brighter during:
- **Receiving commands** from connected device
- **Sending responses** back to connected device  
- **State updates** sent to connected device

The communication pulse is a subtle 100ms fade with minimal brightness increase.

## Technical Notes

- **Advertising timeout**: Controlled by `ADVERTISING_TIMEOUT` constant (30 seconds)
- **Dimmed advertising**: Uses `showBLEAdvertisingDimmed()` function with gentle pulsing range
- **Auto-reset**: Timeout resets when BLE status changes (connect/disconnect)