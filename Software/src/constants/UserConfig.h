#ifndef OSSM_SOFTWARE_USERCONFIG_H
#define OSSM_SOFTWARE_USERCONFIG_H

namespace UserConfig {
    static bool displayMetric = true;
    static float strokeEngineSpeedCurve = 0.8;
    static float afterHomingPosition = 0.0;

    // When true: BLE speed commands (0-100) are treated as a percentage of the
    // current knob value. When false: BLE speed commands (0-100) are used
    // directly as the speed value.

    // MQTT telemetry publish rate in Hz (messages per second).
    // The publish loop will target this rate on a best-effort basis.
    static float mqttPublishFrequencyHz = 30.0f;
}
#endif  // OSSM_SOFTWARE_USERCONFIG_H
