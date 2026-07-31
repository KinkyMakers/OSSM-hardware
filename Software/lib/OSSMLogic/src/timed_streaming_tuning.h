#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace timed_streaming {

    struct TuningParameters {
        uint32_t jerkRampMilliseconds = 200;
        uint32_t primeMilliseconds = 100;
        uint32_t executionHorizonMilliseconds = 72;
        double accelerationScale = 1.0;
        uint32_t momentumDecayMilliseconds = 200;
        double maximumCoastFraction = 0.08;
        double edgeRepulsionStrength = 2.0;
        double centerSpringStrength = 0.35;
    };

    struct TuningBounds {
        static constexpr uint32_t minimumJerkRampMilliseconds = 150;
        static constexpr uint32_t maximumJerkRampMilliseconds = 600;
        static constexpr uint32_t minimumPrimeMilliseconds = 15;
        static constexpr uint32_t maximumPrimeMilliseconds = 120;
        static constexpr uint32_t minimumExecutionHorizonMilliseconds = 32;
        static constexpr uint32_t maximumExecutionHorizonMilliseconds = 72;
        static constexpr double minimumAccelerationScale = 0.25;
        static constexpr double maximumAccelerationScale = 1.0;
        static constexpr uint32_t minimumMomentumDecayMilliseconds = 100;
        static constexpr uint32_t maximumMomentumDecayMilliseconds = 500;
        static constexpr double minimumMaximumCoastFraction = 0.03;
        static constexpr double maximumMaximumCoastFraction = 0.09;
        static constexpr double minimumEdgeRepulsionStrength = 0.5;
        static constexpr double maximumEdgeRepulsionStrength = 4.0;
        static constexpr double minimumCenterSpringStrength = 0.05;
        static constexpr double maximumCenterSpringStrength = 1.0;
    };

    enum class TuningValidationError {
        None,
        JerkRamp,
        Prime,
        ExecutionHorizon,
        AccelerationScale,
        MomentumDecay,
        MaximumCoast,
        EdgeRepulsion,
        CenterSpring,
    };

    inline TuningValidationError validateTuningParameters(
        const TuningParameters &parameters) {
        if (parameters.jerkRampMilliseconds <
                TuningBounds::minimumJerkRampMilliseconds ||
            parameters.jerkRampMilliseconds >
                TuningBounds::maximumJerkRampMilliseconds)
            return TuningValidationError::JerkRamp;
        if (parameters.primeMilliseconds <
                TuningBounds::minimumPrimeMilliseconds ||
            parameters.primeMilliseconds >
                TuningBounds::maximumPrimeMilliseconds)
            return TuningValidationError::Prime;
        if (parameters.executionHorizonMilliseconds <
                TuningBounds::minimumExecutionHorizonMilliseconds ||
            parameters.executionHorizonMilliseconds >
                TuningBounds::maximumExecutionHorizonMilliseconds)
            return TuningValidationError::ExecutionHorizon;
        if (!std::isfinite(parameters.accelerationScale) ||
            parameters.accelerationScale <
                TuningBounds::minimumAccelerationScale ||
            parameters.accelerationScale >
                TuningBounds::maximumAccelerationScale)
            return TuningValidationError::AccelerationScale;
        if (parameters.momentumDecayMilliseconds <
                TuningBounds::minimumMomentumDecayMilliseconds ||
            parameters.momentumDecayMilliseconds >
                TuningBounds::maximumMomentumDecayMilliseconds)
            return TuningValidationError::MomentumDecay;
        if (!std::isfinite(parameters.maximumCoastFraction) ||
            parameters.maximumCoastFraction <
                TuningBounds::minimumMaximumCoastFraction ||
            parameters.maximumCoastFraction >
                TuningBounds::maximumMaximumCoastFraction)
            return TuningValidationError::MaximumCoast;
        if (!std::isfinite(parameters.edgeRepulsionStrength) ||
            parameters.edgeRepulsionStrength <
                TuningBounds::minimumEdgeRepulsionStrength ||
            parameters.edgeRepulsionStrength >
                TuningBounds::maximumEdgeRepulsionStrength)
            return TuningValidationError::EdgeRepulsion;
        if (!std::isfinite(parameters.centerSpringStrength) ||
            parameters.centerSpringStrength <
                TuningBounds::minimumCenterSpringStrength ||
            parameters.centerSpringStrength >
                TuningBounds::maximumCenterSpringStrength)
            return TuningValidationError::CenterSpring;
        return TuningValidationError::None;
    }

    inline bool tuningParametersEqual(const TuningParameters &left,
                                      const TuningParameters &right) {
        return left.jerkRampMilliseconds == right.jerkRampMilliseconds &&
               left.primeMilliseconds == right.primeMilliseconds &&
               left.executionHorizonMilliseconds ==
                   right.executionHorizonMilliseconds &&
               left.accelerationScale == right.accelerationScale &&
               left.momentumDecayMilliseconds ==
                   right.momentumDecayMilliseconds &&
               left.maximumCoastFraction == right.maximumCoastFraction &&
               left.edgeRepulsionStrength == right.edgeRepulsionStrength &&
               left.centerSpringStrength == right.centerSpringStrength;
    }

    inline uint64_t tuningParametersHash(const TuningParameters &parameters) {
        // Hash a canonical fixed-point representation so the host and device
        // can identify an exact configuration without depending on JSON float
        // formatting or structure padding.
        const std::array<uint64_t, 8> values = {
            parameters.jerkRampMilliseconds,
            parameters.primeMilliseconds,
            parameters.executionHorizonMilliseconds,
            static_cast<uint64_t>(
                std::llround(parameters.accelerationScale * 1000000.0)),
            parameters.momentumDecayMilliseconds,
            static_cast<uint64_t>(
                std::llround(parameters.maximumCoastFraction * 1000000.0)),
            static_cast<uint64_t>(
                std::llround(parameters.edgeRepulsionStrength * 1000000.0)),
            static_cast<uint64_t>(
                std::llround(parameters.centerSpringStrength * 1000000.0)),
        };
        uint64_t hash = 1469598103934665603ULL;
        for (uint64_t value : values) {
            for (size_t byte = 0; byte < sizeof(value); ++byte) {
                hash ^= static_cast<uint8_t>(value >> (byte * 8));
                hash *= 1099511628211ULL;
            }
        }
        return hash;
    }

    struct ReferenceSample {
        double positionSteps = 0.0;
        uint32_t durationMilliseconds = 0;
    };

    class ReferenceVelocityEstimator {
      public:
        void reset() { count_ = 0; }

        void record(double positionSteps, uint32_t durationMilliseconds) {
            if (durationMilliseconds == 0) return;
            if (count_ < samples_.size()) {
                samples_[count_++] = {positionSteps, durationMilliseconds};
                return;
            }
            for (size_t index = 1; index < samples_.size(); ++index)
                samples_[index - 1] = samples_[index];
            samples_.back() = {positionSteps, durationMilliseconds};
        }

        size_t count() const { return count_; }

        double velocityStepsPerSecond() const {
            if (count_ < 2) return 0.0;
            std::array<double, 4> times{};
            double elapsed = 0.0;
            for (size_t index = 0; index < count_; ++index) {
                times[index] = elapsed;
                elapsed += samples_[index].durationMilliseconds / 1000.0;
            }
            double meanTime = 0.0;
            double meanPosition = 0.0;
            for (size_t index = 0; index < count_; ++index) {
                meanTime += times[index];
                meanPosition += samples_[index].positionSteps;
            }
            meanTime /= count_;
            meanPosition /= count_;
            double numerator = 0.0;
            double denominator = 0.0;
            for (size_t index = 0; index < count_; ++index) {
                const double timeDelta = times[index] - meanTime;
                numerator +=
                    timeDelta * (samples_[index].positionSteps - meanPosition);
                denominator += timeDelta * timeDelta;
            }
            return denominator > 1e-12 ? numerator / denominator : 0.0;
        }

      private:
        std::array<ReferenceSample, 4> samples_{};
        size_t count_ = 0;
    };

    inline double boundedCoastDisplacement(
        double initialVelocityStepsPerSecond, uint32_t elapsedMilliseconds,
        uint32_t decayMilliseconds, double maximumCoastSteps) {
        if (decayMilliseconds == 0 || maximumCoastSteps <= 0.0 ||
            !std::isfinite(initialVelocityStepsPerSecond))
            return 0.0;
        const double elapsedSeconds = elapsedMilliseconds / 1000.0;
        const double decaySeconds = decayMilliseconds / 1000.0;
        const double displacement = initialVelocityStepsPerSecond *
                                    decaySeconds *
                                    (1.0 - std::exp(-elapsedSeconds /
                                                    decaySeconds));
        return std::max(-maximumCoastSteps,
                        std::min(maximumCoastSteps, displacement));
    }

    inline double quinticBlend(double progress) {
        const double value = std::max(0.0, std::min(1.0, progress));
        return value * value * value *
               (value * (value * 6.0 - 15.0) + 10.0);
    }

}  // namespace timed_streaming
