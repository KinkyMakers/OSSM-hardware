# FTS Streaming Bug Fixes

## Overview

This document describes the changes made to fix FTS (Fleshy Thrust Sync) streaming functionality on the `aj/faptap` branch.

---

## 1. Fixed speed calculation ignoring time parameter

**File:** `src/ossm/OSSM.SimplePenetration.cpp`

**Problem:** The original code calculated the required velocity but immediately overwrote it with `maxSpeed`, causing the device to always move at maximum speed instead of smoothly over the requested time interval.

**Before:**

```cpp
targetVelocity = 2.0f * deltaX / t - v0;
targetVelocity = maxSpeed;  // ← Overwrote calculation!
ossm->stepper->setSpeedInHz(maxSpeed);
```

**After:**

```cpp
float requiredSpeed = (distance / timeSeconds) * 1.5f;
requiredSpeed = constrain(requiredSpeed, 100.0f, maxSpeed);
ossm->stepper->setSpeedInHz(static_cast<uint32_t>(requiredSpeed));
```

---

## 2. Fixed position mapping direction

**File:** `src/ossm/OSSM.SimplePenetration.cpp`

**Problem:** Original used `180 - position` which inverted the meaning (FTS 0 = extended). Changed to direct mapping so FTS 0 = retracted and FTS 180 = extended, matching funscript convention where `pos: 0` = bottom and `pos: 100` = top.

**Before:**

```cpp
targetPosition = -(180 - targetPositionTime.position) / 180.0f * stroke;
// FTS 0 = extended (wrong)
```

**After:**

```cpp
targetPosition = -(targetPositionTime.position / 180.0f) * stroke;
// FTS 0 = retracted, FTS 180 = extended (correct)
```

---

## 3. Replaced deadband filter with explicit update flag

**Files:** `queue.h`, `queue.cpp`, `nimble.cpp`

**Problem:** The original `hasTargetChanged()` function compared against `lastPositionTime` but the streaming task never updated it after consuming a command. This caused commands with the same time value (like alternating 500ms strokes) to be filtered out.

**Before:**

```cpp
// queue.cpp
bool hasTargetChanged() {
    return abs(lastPositionTime.position - targetPositionTime.position) > 2 ||
           abs(lastPositionTime.inTime - targetPositionTime.inTime) > 10;
}
```

**After:**

```cpp
// queue.cpp
bool targetUpdated = false;

void markTargetUpdated() {
    targetUpdated = true;
}

bool consumeTargetUpdate() {
    if (targetUpdated) {
        targetUpdated = false;
        return true;
    }
    return false;
}
```

**Usage:**

- `markTargetUpdated()` — called by BLE callback when new command arrives
- `consumeTargetUpdate()` — called by streaming task, returns true once per command

---

## 4. Increased speed multiplier from 1.2× to 1.5×

**File:** `src/ossm/OSSM.SimplePenetration.cpp`

**Problem:** The simple `v = d/t` calculation doesn't account for acceleration/deceleration phases. A 1.5× multiplier ensures the motor reaches the target position within the allotted time despite ramping.

```cpp
float requiredSpeed = (distance / timeSeconds) * 1.5f;
```

---

## 5. Changed logging from DEBUG to INFO level

**Files:** `src/ossm/OSSM.SimplePenetration.cpp`, `nimble.cpp`

**Problem:** `ESP_LOGD` is often compiled out or disabled. Changed to `ESP_LOGI` so FTS commands and motion parameters appear in the serial monitor for debugging.

```cpp
ESP_LOGI("Streaming", "Pos: %d -> %.0f, Dist: %.0f, Time: %.3fs, Speed: %.0f",
         targetPositionTime.position, targetPosition, distance, timeSeconds, requiredSpeed);
```

---

## 6. Removed unused variables

**File:** `src/ossm/OSSM.SimplePenetration.cpp`

**Problem:** Several variables were declared but never used after the refactor.

**Removed:**

- `currentVelocity`
- `targetVelocity`
- `speedMultipliers[]`
- `speedMultiplierIndex`
- `RMS_position`
- `rms_samples`
- `lastTime`
- `loopStart`

---

## Files Modified

| File                                    | Changes                                                           |
| --------------------------------------- | ----------------------------------------------------------------- |
| `src/ossm/OSSM.SimplePenetration.cpp`   | Speed calculation, position mapping, logging, cleanup             |
| `src/services/communication/queue.h`    | Added `markTargetUpdated()`, `consumeTargetUpdate()` declarations |
| `src/services/communication/queue.cpp`  | Implemented flag-based update detection                           |
| `src/services/communication/nimble.cpp` | Call `markTargetUpdated()` on FTS write, improved logging         |
