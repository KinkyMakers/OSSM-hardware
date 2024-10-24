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


inline std::string getSettingName(AdvancedConfigurationSettingName name) {
    //Max 15 characters for use as NVS key
    switch (name) {
        case AdvancedConfigurationSettingName::ToggleHomeDirection: return "ToggleHomeDir";
        case AdvancedConfigurationSettingName::StepsPerRev: return "StepsPerRev";
        case AdvancedConfigurationSettingName::PulleyTeeth: return "PulleyTeeth";
        case AdvancedConfigurationSettingName::MaxSpeed: return "MaxSpeed";
        case AdvancedConfigurationSettingName::RailLength: return "RailLength";
        case AdvancedConfigurationSettingName::RapidHoming: return "RapidHoming";
        case AdvancedConfigurationSettingName::HomingCurrentLimit: return "HomingCurLimit";
        default: return "";
    }
}

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
    std::optional<float> value;
};

struct SavedSettings {
    std::vector<SavedSetting> savedValues;
};

#endif  // SOFTWARE_ADVANCEDCONFIGURATIONSETTINGS_H
