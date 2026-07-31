#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace timed_streaming {

    enum class SubmissionDisposition {
        Commit,
        Retry,
        UnexpectedEmpty,
        Fatal,
    };

    // FastAccelStepper result values are passed in so this pure policy remains
    // independently testable and does not couple OSSMLogic to the driver
    // library.
    inline SubmissionDisposition classifySubmission(int result, int emptyResult,
                                                    bool priming) {
        if (result == 0) return SubmissionDisposition::Commit;
        if (result == emptyResult) {
            return priming ? SubmissionDisposition::Commit
                           : SubmissionDisposition::UnexpectedEmpty;
        }
        return result > 0 ? SubmissionDisposition::Retry
                          : SubmissionDisposition::Fatal;
    }

    inline uint32_t requiredPrimeMilliseconds(
        bool latencyCompensation, float bufferSetting,
        uint32_t minimumMilliseconds = 8, uint32_t maximumMilliseconds = 200) {
        if (!latencyCompensation) return minimumMilliseconds;
        const auto requested =
            static_cast<uint32_t>(std::max(0.0f, bufferSetting) * 2.0f);
        return std::max(minimumMilliseconds,
                        std::min(maximumMilliseconds, requested));
    }

    inline int32_t playRangeCenterSteps(int32_t minimumSteps,
                                        int32_t maximumSteps) {
        if (minimumSteps > maximumSteps) std::swap(minimumSteps, maximumSteps);
        return static_cast<int32_t>(
            static_cast<int64_t>(minimumSteps) +
            (static_cast<int64_t>(maximumSteps) - minimumSteps) / 2);
    }

    // Starvation recovery intentionally uses only a quarter of the configured
    // speed. The lower bound gives the jerk-limited follower time to settle;
    // the distance term prevents a long play range from becoming an aggressive
    // dash toward center.
    inline uint32_t centerRecoveryDurationMilliseconds(
        int32_t currentSteps, int32_t centerSteps,
        double speedLimitStepsPerSecond, uint32_t minimumMilliseconds = 750,
        uint32_t maximumMilliseconds = 5000) {
        if (minimumMilliseconds > maximumMilliseconds)
            std::swap(minimumMilliseconds, maximumMilliseconds);
        const double distance =
            std::abs(static_cast<double>(static_cast<int64_t>(centerSteps) -
                                         static_cast<int64_t>(currentSteps)));
        const double recoverySpeed =
            std::max(1.0, std::abs(speedLimitStepsPerSecond) * 0.25);
        const auto distanceMilliseconds =
            static_cast<uint32_t>(std::ceil(std::min<double>(
                maximumMilliseconds, distance * 1000.0 / recoverySpeed)));
        return std::max(minimumMilliseconds,
                        std::min(maximumMilliseconds, distanceMilliseconds));
    }

    enum class ResetCause { SpeedZero, InputOverflow, Underrun, FatalError };

    struct ResetPolicy {
        bool forceStop;
        bool clearInputQueue;
        bool preserveActiveWaypoint;
        bool rebuffer;
    };

    inline ResetPolicy resetPolicy(ResetCause cause) {
        switch (cause) {
            case ResetCause::InputOverflow:
                // The fixed input queue replaces only its oldest entry. Keep
                // the active jerk-limited trajectory running toward the
                // retained, newer request.
                return {false, false, true, false};
            case ResetCause::Underrun:
                return {true, false, true, true};
            case ResetCause::SpeedZero:
            case ResetCause::FatalError:
                return {true, true, false, false};
        }
        return {true, true, false, false};
    }

}  // namespace timed_streaming
