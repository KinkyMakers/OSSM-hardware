#pragma once

#ifndef OSSM_ENABLE_RAD_BLE
#define OSSM_ENABLE_RAD_BLE 1
#endif

#ifndef OSSM_HARDWARE_VARIANT
#define OSSM_HARDWARE_VARIANT "v2"
#endif

namespace ossm_hardware {

inline constexpr const char *HARDWARE_VARIANT = OSSM_HARDWARE_VARIANT;
inline constexpr bool RAD_BLE_ENABLED = OSSM_ENABLE_RAD_BLE == 1;

static_assert(OSSM_ENABLE_RAD_BLE == 0 || OSSM_ENABLE_RAD_BLE == 1,
              "OSSM_ENABLE_RAD_BLE must be 0 or 1");

}  // namespace ossm_hardware
