#ifndef SOFTWARE_SETTINGPERCENTS_H
#define SOFTWARE_SETTINGPERCENTS_H

#include <optional>

enum class StrokePatterns {
    SimpleStroke,
    TeasingPounding,
    RoboStroke,
    HalfnHalf,
    Deeper,
    StopNGo,
    Insist,
};

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
