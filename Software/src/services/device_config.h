#ifndef OSSM_SOFTWARE_DEVICE_CONFIG_H
#define OSSM_SOFTWARE_DEVICE_CONFIG_H

#include "device_config_logic.h"

// Settings persisted to NVS (namespace "ossm-cfg").
//
// Live settings apply immediately and are kept in their existing globals
// (UserConfig::*, USE_SPEED_KNOB_AS_LIMIT, USE_LATENCY_COMPENSATION).
// Machine geometry is read once at boot; saved changes apply after restart.
// Motion settings (speed, stroke, depth, ...) are never persisted so the
// machine always boots at zero speed and passes preflight.
namespace device_config {

// Call once at boot, before motion, BLE or the state machine start.
void load();

// Geometry in use since boot.
const device_config_logic::MachineConfig &active();
// Geometry saved in NVS; equals active() unless a restart is pending.
const device_config_logic::MachineConfig &stored();
bool restartRequired();

// Validates, saves and (on success) replaces stored(). Takes effect on the
// next boot.
device_config_logic::GeometryError saveMachineConfig(
    const device_config_logic::MachineConfig &config);

// Live settings: validate, apply, then save. Return false when the value is
// invalid or could not be stored.
bool setDisplayMetric(bool value);
bool setAfterHomingPosition(float value);
bool setSpeedKnobAsLimit(bool value);
// Session-only: used by the legacy BLE characteristic so a remote connect
// does not overwrite the saved preference.
void applySpeedKnobAsLimit(bool value);
bool setLatencyCompensation(bool value);
bool setMqttPublishFrequencyHz(float value);

}  // namespace device_config

#endif  // OSSM_SOFTWARE_DEVICE_CONFIG_H
