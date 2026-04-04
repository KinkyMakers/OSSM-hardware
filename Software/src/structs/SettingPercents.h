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

static constexpr int HARDCODED_PATTERN_COUNT = 7;

struct SettingPercents {
    float speed;
    float stroke;
    float sensation;
    float depth;
    float buffer;
    int pattern;
    float speedKnob;
    std::optional<float> speedBLE = std::nullopt;
};

#endif  // SOFTWARE_SETTINGPERCENTS_H
