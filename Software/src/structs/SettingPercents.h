#ifndef SOFTWARE_SETTINGPERCENTS_H
#define SOFTWARE_SETTINGPERCENTS_H

#include "../lib/StrokeEngine/src/StrokeEngine.h"

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
