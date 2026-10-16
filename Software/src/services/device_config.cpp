#include "device_config.h"

#include <Preferences.h>
#include <RadBle.h>

#include "constants/Config.h"
#include "constants/UserConfig.h"
#include "esp_log.h"
#include "services/board.h"

using device_config_logic::GeometryError;
using device_config_logic::MachineConfig;

namespace device_config {

namespace {

constexpr const char *TAG = "CONFIG";
constexpr const char *NAMESPACE = "ossm-cfg";

// NVS keys are limited to 15 characters. Renaming a key loses its value.
constexpr const char *KEY_STEPS_PER_REV = "stepsPerRev";
constexpr const char *KEY_PULLEY_TEETH = "pulleyTeeth";
constexpr const char *KEY_BELT_PITCH = "beltPitchMm";
constexpr const char *KEY_MAX_RPM = "maxRpm";
constexpr const char *KEY_MAX_STROKE = "maxStrokeMm";
constexpr const char *KEY_METRIC = "metric";
constexpr const char *KEY_HOME_POSITION = "homePosition";
constexpr const char *KEY_KNOB_LIMIT = "knobLimit";
constexpr const char *KEY_LATENCY = "latencyComp";
constexpr const char *KEY_MQTT_HZ = "mqttHz";

MachineConfig activeConfig;
MachineConfig storedConfig;

template <typename Write>
bool writeNvs(Write write) {
    Preferences preferences;
    if (!preferences.begin(NAMESPACE, false)) {
        ESP_LOGE(TAG, "Could not open NVS namespace %s", NAMESPACE);
        return false;
    }
    const bool stored = write(preferences);
    preferences.end();
    if (!stored) ESP_LOGE(TAG, "NVS write failed");
    return stored;
}

bool putFloat(const char *key, float value) {
    return writeNvs([&](Preferences &preferences) {
        return preferences.putFloat(key, value) == sizeof(float);
    });
}

bool putBool(const char *key, bool value) {
    return writeNvs([&](Preferences &preferences) {
        return preferences.putBool(key, value) == sizeof(bool);
    });
}

// Missing keys are written with their default, so a fresh device ends up with
// a complete namespace and later boots do no writes.
float loadFloat(Preferences &preferences, const char *key, float fallback) {
    if (preferences.isKey(key)) return preferences.getFloat(key, fallback);
    if (preferences.putFloat(key, fallback) == sizeof(float))
        ESP_LOGI(TAG, "Stored default %s = %g", key, fallback);
    else
        ESP_LOGE(TAG, "Could not store default %s", key);
    return fallback;
}

bool loadBool(Preferences &preferences, const char *key, bool fallback) {
    if (preferences.isKey(key)) return preferences.getBool(key, fallback);
    if (preferences.putBool(key, fallback) == sizeof(bool))
        ESP_LOGI(TAG, "Stored default %s = %d", key, fallback);
    else
        ESP_LOGE(TAG, "Could not store default %s", key);
    return fallback;
}

MachineConfig readMachineConfig(Preferences &preferences) {
    const MachineConfig defaults;
    MachineConfig config;
    config.stepsPerRev =
        loadFloat(preferences, KEY_STEPS_PER_REV, defaults.stepsPerRev);
    config.pulleyToothCount =
        loadFloat(preferences, KEY_PULLEY_TEETH, defaults.pulleyToothCount);
    config.beltPitchMm =
        loadFloat(preferences, KEY_BELT_PITCH, defaults.beltPitchMm);
    config.maxRpm = loadFloat(preferences, KEY_MAX_RPM, defaults.maxRpm);
    config.maxStrokeMm =
        loadFloat(preferences, KEY_MAX_STROKE, defaults.maxStrokeMm);

    const GeometryError error = device_config_logic::validateGeometry(
        config, Config::Driver::maxAcceleration);
    if (error != GeometryError::None) {
        ESP_LOGE(TAG, "Stored machine config rejected (%s); using defaults",
                 device_config_logic::geometryErrorMessage(error));
        return defaults;
    }
    return config;
}

void readLiveSettings(Preferences &preferences) {
    UserConfig::displayMetric =
        loadBool(preferences, KEY_METRIC, UserConfig::displayMetric);
    USE_SPEED_KNOB_AS_LIMIT =
        loadBool(preferences, KEY_KNOB_LIMIT, USE_SPEED_KNOB_AS_LIMIT);
    USE_LATENCY_COMPENSATION =
        loadBool(preferences, KEY_LATENCY, USE_LATENCY_COMPENSATION);

    const float homePosition = loadFloat(preferences, KEY_HOME_POSITION,
                                         UserConfig::afterHomingPosition);
    if (device_config_logic::isValidAfterHomingPosition(homePosition))
        UserConfig::afterHomingPosition = homePosition;

    const float mqttHz = loadFloat(preferences, KEY_MQTT_HZ,
                                   UserConfig::mqttPublishFrequencyHz);
    if (device_config_logic::isValidMqttPublishHz(mqttHz))
        UserConfig::mqttPublishFrequencyHz = mqttHz;
}

// Names set before rad-ble existed live in UserConfig/DeviceName. Copy one
// into rad-ble storage once, then drop the old key.
void migrateLegacyDeviceName() {
    Preferences legacy;
    if (!legacy.begin("UserConfig", true)) return;
    const bool hasLegacyName = legacy.isKey("DeviceName");
    const String legacyName = legacy.getString("DeviceName", "");
    legacy.end();
    if (!hasLegacyName || !legacy.begin("UserConfig", false)) return;

    bool radBleHasName = false;
    Preferences radBle;
    if (radBle.begin("radble", true)) {
        radBleHasName = radBle.isKey("deviceName");
        radBle.end();
    }

    if (!radBleHasName && !legacyName.isEmpty()) {
        if (!radble::persistDeviceName(legacyName)) {
            ESP_LOGE(TAG, "Could not migrate device name; keeping old key");
            legacy.end();
            return;
        }
        ESP_LOGI(TAG, "Migrated device name \"%s\"", legacyName.c_str());
    }
    legacy.remove("DeviceName");
    legacy.end();
}

}  // namespace

void load() {
    migrateLegacyDeviceName();

    Preferences preferences;
    // Read-write so a fresh device gets the namespace and its defaults.
    if (preferences.begin(NAMESPACE, false)) {
        storedConfig = readMachineConfig(preferences);
        readLiveSettings(preferences);
        preferences.end();
    } else {
        ESP_LOGE(TAG, "Could not open NVS namespace %s; using defaults",
                 NAMESPACE);
        storedConfig = MachineConfig{};
    }
    activeConfig = storedConfig;

    ESP_LOGI(TAG,
             "Machine: %.0f steps/rev, %.0f teeth, %.2f mm pitch, %.0f rpm, "
             "%.0f mm stroke (%.2f steps/mm, %.0f steps/s max)",
             activeConfig.stepsPerRev, activeConfig.pulleyToothCount,
             activeConfig.beltPitchMm, activeConfig.maxRpm,
             activeConfig.maxStrokeMm,
             device_config_logic::stepsPerMm(activeConfig),
             device_config_logic::maxStepRateHz(activeConfig));
}

const MachineConfig &active() { return activeConfig; }

const MachineConfig &stored() { return storedConfig; }

bool restartRequired() {
    return activeConfig.stepsPerRev != storedConfig.stepsPerRev ||
           activeConfig.pulleyToothCount != storedConfig.pulleyToothCount ||
           activeConfig.beltPitchMm != storedConfig.beltPitchMm ||
           activeConfig.maxRpm != storedConfig.maxRpm ||
           activeConfig.maxStrokeMm != storedConfig.maxStrokeMm;
}

GeometryError saveMachineConfig(const MachineConfig &config) {
    const GeometryError error = device_config_logic::validateGeometry(
        config, Config::Driver::maxAcceleration);
    if (error != GeometryError::None) return error;

    const bool stored = writeNvs([&](Preferences &preferences) {
        const size_t size = sizeof(float);
        return preferences.putFloat(KEY_STEPS_PER_REV, config.stepsPerRev) ==
                   size &&
               preferences.putFloat(KEY_PULLEY_TEETH,
                                    config.pulleyToothCount) == size &&
               preferences.putFloat(KEY_BELT_PITCH, config.beltPitchMm) ==
                   size &&
               preferences.putFloat(KEY_MAX_RPM, config.maxRpm) == size &&
               preferences.putFloat(KEY_MAX_STROKE, config.maxStrokeMm) ==
                   size;
    });
    // A partial write is caught at boot: readMachineConfig validates the
    // combination and falls back to defaults.
    if (!stored) return GeometryError::StorageFailed;

    storedConfig = config;
    ESP_LOGI(TAG, "Machine config saved; restart to apply");
    return GeometryError::None;
}

// Flash writes stall the cache, so unchanged values are not rewritten.

bool setDisplayMetric(bool value) {
    if (UserConfig::displayMetric == value) return true;
    UserConfig::displayMetric = value;
    return putBool(KEY_METRIC, value);
}

bool setAfterHomingPosition(float value) {
    if (!device_config_logic::isValidAfterHomingPosition(value)) return false;
    if (UserConfig::afterHomingPosition == value) return true;
    UserConfig::afterHomingPosition = value;
    return putFloat(KEY_HOME_POSITION, value);
}

void applySpeedKnobAsLimit(bool value) { USE_SPEED_KNOB_AS_LIMIT = value; }

bool setSpeedKnobAsLimit(bool value) {
    if (USE_SPEED_KNOB_AS_LIMIT == value) return true;
    USE_SPEED_KNOB_AS_LIMIT = value;
    return putBool(KEY_KNOB_LIMIT, value);
}

bool setLatencyCompensation(bool value) {
    if (USE_LATENCY_COMPENSATION == value) return true;
    USE_LATENCY_COMPENSATION = value;
    return putBool(KEY_LATENCY, value);
}

bool setMqttPublishFrequencyHz(float value) {
    if (!device_config_logic::isValidMqttPublishHz(value)) return false;
    if (UserConfig::mqttPublishFrequencyHz == value) return true;
    UserConfig::mqttPublishFrequencyHz = value;
    return putFloat(KEY_MQTT_HZ, value);
}

}  // namespace device_config
