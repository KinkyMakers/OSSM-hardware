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
};

struct SettingPercents {
    float speed;
    float stroke;
    float sensation;
    float depth;
    StrokePatterns pattern;
    float speedKnob;
};

#endif  // SOFTWARE_SETTINGPERCENTS_H
