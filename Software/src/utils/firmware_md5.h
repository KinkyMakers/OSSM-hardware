#ifndef OSSM_UTILS_FIRMWARE_MD5_H
#define OSSM_UTILS_FIRMWARE_MD5_H

#include <Arduino.h>

// Computes the running sketch MD5 once, at boot, while the heap is still
// large. ESP.getSketchMD5() allocates a multi-KB buffer with new[]; called
// later from the NimBLE host task (pairing characteristic read) or the
// pairing poll, that allocation fails under BLE+MQTT+Wi-Fi and the C++
// runtime aborts the device (seen on 1.0.51, 1.0.58 and this branch,
// 2026-09-09). Call cacheSketchMd5() before starting communications.
void cacheSketchMd5();
const String &sketchMd5();

#endif  // OSSM_UTILS_FIRMWARE_MD5_H
