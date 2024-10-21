#ifndef SOFTWARE_ADVANCEDCONFIGURATIONSETTINGS_H
#define SOFTWARE_ADVANCEDCONFIGURATIONSETTINGS_H

#include <vector>
#include <optional>

enum class DataType {
    NUMBER,
    BOOL,
    INFO,
    ACTION
};

enum AdvancedConfigurationSettingName {
    ReadMe,
    //Add additional settings below this line only
    ToggleHomeDirection,
    StepsPerRev,
    PulleyTeeth,
    MaxSpeed,
    RailLength,
    RapidHoming,
    HomingCurrentLimit,
    //Add additional settings above this line only
    ResetToDefaults,
    Apply,
    SettingCount
};

struct Setting {
    AdvancedConfigurationSettingName name;
    DataType type;
    float minValue;
    float maxValue;
    float settingPrecision;
    float defaultValue;
    std::optional<float> savedValue;
    float currentValue() const {
        return savedValue.value_or(defaultValue);
    }
};

struct AdvancedConfigurationSettings {
    std::vector<Setting> settings;
};

struct SavedSetting {
    AdvancedConfigurationSettingName name;
    float value;
};

struct SavedSettings {
    std::vector<SavedSetting> savedValues;
};

#endif  // SOFTWARE_ADVANCEDCONFIGURATIONSETTINGS_H
