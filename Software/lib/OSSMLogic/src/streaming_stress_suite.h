#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace streaming_stress {

enum class Pattern : uint8_t { Sine, Triangle, Random };
enum class Delivery : uint8_t {
    Regular,
    Jitter,
    Gaps,
    Bursts,
    CadencePressure,
    Irregular,
    MicroMacro,
};

struct Profile {
    const char* id;
    uint8_t minimum;
    uint8_t maximum;
    uint16_t frequencyMilliHz;
    uint8_t cadenceHz;
    uint8_t speed;
    uint8_t stroke;
    uint8_t depth;
    uint8_t sensation;
    uint8_t buffer;
    bool latencyCompensation;
};

struct FaultCase {
    const char* id;
    Pattern pattern;
    Delivery delivery;
    uint8_t profileIndex;
    uint32_t seed;
};

}  // namespace streaming_stress

#include "streaming_stress_suite.generated.h"

namespace streaming_stress {

struct Scenario {
    const char* id;
    Pattern pattern;
    Delivery delivery;
    const Profile* profile;
    uint32_t seed;
    bool repeat;
};

inline const char* patternName(Pattern pattern) {
    switch (pattern) {
        case Pattern::Sine: return "sine";
        case Pattern::Triangle: return "triangle";
        case Pattern::Random: return "random";
    }
    return "unknown";
}

inline Scenario scenarioAt(size_t index) {
    if (index < kCoreCaseCount) {
        size_t patternIndex = index / kProfileCount;
        size_t profileIndex = index % kProfileCount;
        Pattern pattern = static_cast<Pattern>(patternIndex);
        return {kProfiles[profileIndex].id, pattern, Delivery::Regular,
                &kProfiles[profileIndex], kRandomSeed + static_cast<uint32_t>(index), false};
    }
    index -= kCoreCaseCount;
    if (index < kFaultCount) {
        const auto& fault = kFaults[index];
        return {fault.id, fault.pattern, fault.delivery,
                &kProfiles[fault.profileIndex], fault.seed, false};
    }
    index -= kFaultCount;
    Pattern pattern = static_cast<Pattern>(index);
    return {"nominal_repeat", pattern, Delivery::Regular, &kProfiles[0],
            kRandomSeed + 100u + static_cast<uint32_t>(index), true};
}

inline float unitWave(Pattern pattern, uint32_t timeMs,
                      uint16_t frequencyMilliHz, uint32_t seed) {
    constexpr float kPi = 3.14159265358979323846f;
    float seconds = timeMs / 1000.0f;
    float hz = frequencyMilliHz / 1000.0f;
    float cycles = seconds * hz;
    if (pattern == Pattern::Sine) return std::sin(2.0f * kPi * cycles);
    if (pattern == Pattern::Triangle) {
        float phase = cycles - std::floor(cycles);
        return 1.0f - 4.0f * std::fabs(phase - 0.5f);
    }
    auto phase = [seed, kPi](uint32_t shift) {
        uint32_t mixed = seed * 1664525u + 1013904223u + shift * 2246822519u;
        return (mixed & 0xffffu) / 65535.0f * 2.0f * kPi;
    };
    return 0.45f * std::sin(2.0f * kPi * cycles + phase(1)) +
           0.25f * std::sin(2.0f * kPi * cycles * 1.7f + phase(2)) +
           0.20f * std::sin(2.0f * kPi * cycles * 0.47f + phase(3)) +
           0.10f * std::sin(2.0f * kPi * cycles * 2.3f + phase(4));
}

inline float microMacroUnit(uint32_t timeMs) {
    constexpr uint16_t kDurationsMs[] = {600, 200, 900, 300, 700};
    constexpr float kTargets[] = {0.0f, -0.08f, 0.90f, 0.05f, -0.75f, 0.10f};
    constexpr uint32_t kPeriodMs = 2700;
    uint32_t phaseMs = timeMs % kPeriodMs;
    uint32_t segmentStart = 0;
    for (size_t index = 0; index < 5; ++index) {
        uint32_t segmentEnd = segmentStart + kDurationsMs[index];
        if (phaseMs < segmentEnd) {
            float t = static_cast<float>(phaseMs - segmentStart) /
                      static_cast<float>(kDurationsMs[index]);
            float smooth = t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
            return kTargets[index] + (kTargets[index + 1] - kTargets[index]) * smooth;
        }
        segmentStart = segmentEnd;
    }
    return kTargets[5];
}

inline float requestedPosition(const Scenario& scenario, uint32_t timeMs) {
    const Profile& profile = *scenario.profile;
    float center = (profile.minimum + profile.maximum) / 2.0f;
    float amplitude = (profile.maximum - profile.minimum) / 2.0f;
    float wave = scenario.delivery == Delivery::MicroMacro
        ? microMacroUnit(timeMs)
        : unitWave(scenario.pattern, timeMs, profile.frequencyMilliHz, scenario.seed);
    float value = center + amplitude * wave;
    return std::max<float>(profile.minimum, std::min<float>(profile.maximum, value));
}

inline uint16_t regularIntervalMs(const Scenario& scenario) {
    return static_cast<uint16_t>(1000u / std::max<uint8_t>(1, scenario.profile->cadenceHz));
}

inline bool inDeliveryGap(const Scenario& scenario, uint32_t timeMs) {
    return scenario.delivery == Delivery::Gaps &&
           ((timeMs >= 7800 && timeMs < 8000) ||
            (timeMs >= 11800 && timeMs < 12000));
}

inline int16_t deterministicJitterMs(const Scenario& scenario, uint32_t commandIndex) {
    if (scenario.delivery != Delivery::Jitter) return 0;
    uint32_t mixed = scenario.seed ^ (commandIndex * 747796405u + 2891336453u);
    return static_cast<int16_t>(mixed % 41u) - 20;
}

inline uint16_t irregularIntervalMs(uint32_t commandIndex) {
    constexpr uint16_t intervals[] = {25, 40, 75, 125, 200};
    return intervals[commandIndex % 5];
}

}  // namespace streaming_stress
