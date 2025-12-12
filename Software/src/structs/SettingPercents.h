#ifndef SOFTWARE_SETTINGPERCENTS_H
#define SOFTWARE_SETTINGPERCENTS_H

enum class StrokePatterns {
    SimpleStroke,
    TeasingPounding,
    RoboStroke,
    HalfnHalf,
    Deeper,
    StopNGo,
    Insist,
    TestPattern1,
    TestPattern2,
};

struct SettingPercents {
    float speed;
    float stroke;
    float sensation;
    float depth;
    StrokePatterns pattern;
    float speedKnob;
    std::optional<float> speedBLE = std::nullopt;
};

#endif  // SOFTWARE_SETTINGPERCENTS_H
