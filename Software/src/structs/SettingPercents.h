#ifndef SOFTWARE_SETTINGPERCENTS_H
#define SOFTWARE_SETTINGPERCENTS_H

struct SettingPercents {
    float speed;
    float stroke;
    float sensation;
    float depth;
    float buffer;
    StrokePatterns pattern;
    float speedKnob;
    std::optional<float> speedBLE = std::nullopt;
};

#endif  // SOFTWARE_SETTINGPERCENTS_H
