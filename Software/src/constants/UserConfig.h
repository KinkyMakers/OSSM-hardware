#ifndef OSSM_SOFTWARE_USERCONFIG_H
#define OSSM_SOFTWARE_USERCONFIG_H

namespace UserConfig {
    extern bool displayMetric;
    extern float afterHomingPosition;

    // When true: BLE speed commands (0-100) are treated as a percentage of the
    // current knob value. When false: BLE speed commands (0-100) are used
    // directly as the speed value.

    // MQTT telemetry publish rate in Hz (messages per second).
    // The publish loop will target this rate on a best-effort basis.
    extern float mqttPublishFrequencyHz;
}
#endif  // OSSM_SOFTWARE_USERCONFIG_H
