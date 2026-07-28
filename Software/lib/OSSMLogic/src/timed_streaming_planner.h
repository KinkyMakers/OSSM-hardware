#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace timed_streaming {

    constexpr uint32_t kSliceMilliseconds = 4;
    constexpr uint32_t kJerkRampMilliseconds = 40;
    constexpr size_t kWaypointCapacity = 64;

    struct Limits {
        double speedStepsPerSecond;
        double accelerationStepsPerSecondSquared;
    };

    struct Range {
        int32_t minimumSteps = std::numeric_limits<int32_t>::min();
        int32_t maximumSteps = std::numeric_limits<int32_t>::max();
        int32_t guardSteps = 0;
    };

    struct Waypoint {
        int32_t positionSteps = 0;
        uint64_t durationTicks = 0;
    };

    struct State {
        int32_t positionSteps = 0;
        double continuousPositionSteps = 0.0;
        double velocityStepsPerSecond = 0.0;
        double accelerationStepsPerSecondSquared = 0.0;
        double fractionalSteps = 0.0;
        int64_t timingCarryTicks = 0;
    };

    struct Reference {
        double positionSteps = 0.0;
        double velocityStepsPerSecond = 0.0;
        double accelerationStepsPerSecondSquared = 0.0;
    };

    struct Slice {
        int16_t steps = 0;
        uint32_t requestedTicks = 0;
        uint32_t nominalTicks = 0;
        State next{};
        Reference reference{};
        uint64_t remainingWaypointTicks = 0;
        bool finishesWaypoint = false;
        bool holdSlice = false;
        bool boundaryEnvelopeActive = false;
        bool safetyClampActive = false;
        double referenceErrorSteps = 0.0;
        double minimumRangeMarginSteps = 0.0;
    };

    // Sequential transactional follower for FastAccelStepper::moveTimed().
    // A preview never mutates state, so driver retries cannot duplicate motion
    // or consume reference time.
    class Planner {
      public:
        explicit Planner(uint32_t ticksPerSecond)
            : ticksPerSecond_(ticksPerSecond) {}

        void reset(int32_t positionSteps) {
            state_ = {};
            state_.positionSteps = positionSteps;
            state_.continuousPositionSteps = positionSteps;
            elapsedWaypointTicks_ = 0;
            waypointCount_ = 0;
        }

        void setRange(int32_t minimumSteps, int32_t maximumSteps,
                      int32_t guardSteps) {
            if (minimumSteps > maximumSteps)
                std::swap(minimumSteps, maximumSteps);
            range_.minimumSteps = minimumSteps;
            range_.maximumSteps = maximumSteps;
            const int64_t span = static_cast<int64_t>(maximumSteps) -
                                 static_cast<int64_t>(minimumSteps);
            range_.guardSteps = static_cast<int32_t>(
                std::max<int64_t>(0, std::min<int64_t>(guardSteps, span / 2)));
        }

        const Range &range() const { return range_; }

        bool appendWaypoint(int32_t targetSteps, uint64_t durationTicks) {
            if (durationTicks == 0 || waypointCount_ == kWaypointCapacity)
                return false;
            const int32_t innerMinimum =
                range_.minimumSteps + range_.guardSteps;
            const int32_t innerMaximum =
                range_.maximumSteps - range_.guardSteps;
            waypoints_[waypointCount_++] = {
                clampInt(targetSteps, innerMinimum, innerMaximum),
                durationTicks};
            return true;
        }

        void beginWaypoint(int32_t targetSteps, uint64_t durationTicks) {
            appendWaypoint(targetSteps, durationTicks);
        }

        bool hasWaypoint() const { return waypointCount_ != 0; }
        size_t waypointCount() const { return waypointCount_; }
        bool canBufferWaypoint() const {
            return waypointCount_ < kWaypointCapacity;
        }
        uint64_t bufferedTicks() const {
            uint64_t total = 0;
            for (size_t index = 0; index < waypointCount_; ++index)
                total += waypoints_[index].durationTicks;
            return total > elapsedWaypointTicks_ ? total - elapsedWaypointTicks_
                                                 : 0;
        }
        uint64_t remainingWaypointTicks() const {
            if (!hasWaypoint()) return 0;
            return waypoints_[0].durationTicks > elapsedWaypointTicks_
                       ? waypoints_[0].durationTicks - elapsedWaypointTicks_
                       : 0;
        }
        int32_t targetSteps() const {
            return hasWaypoint() ? waypoints_[0].positionSteps
                                 : state_.positionSteps;
        }
        const State &state() const { return state_; }
        bool isStationary(double velocityTolerance = 0.5,
                          double accelerationTolerance = 0.5) const {
            return std::abs(state_.velocityStepsPerSecond) <=
                       velocityTolerance &&
                   std::abs(state_.accelerationStepsPerSecondSquared) <=
                       accelerationTolerance;
        }

        void reconcilePosition(int32_t queuedEndpointSteps) {
            const int32_t delta = queuedEndpointSteps - state_.positionSteps;
            state_.positionSteps = queuedEndpointSteps;
            state_.continuousPositionSteps += delta;
        }

        // Retains pending waypoints. Used after a genuine queue stop, never for
        // ordinary packet arrival.
        void stopAndResynchronize(int32_t actualPositionSteps) {
            state_.positionSteps = actualPositionSteps;
            state_.continuousPositionSteps = actualPositionSteps;
            state_.velocityStepsPerSecond = 0.0;
            state_.accelerationStepsPerSecondSquared = 0.0;
            state_.fractionalSteps = 0.0;
            state_.timingCarryTicks = 0;
        }

        Reference referenceAt(uint64_t) const {
            if (!hasWaypoint())
                return {state_.continuousPositionSteps, 0.0, 0.0};
            const double remainingSeconds =
                ticksToSeconds(std::max<uint64_t>(1, remainingWaypointTicks()));
            const double error =
                waypoints_[0].positionSteps - state_.continuousPositionSteps;
            return {static_cast<double>(waypoints_[0].positionSteps),
                    error / remainingSeconds, 0.0};
        }

        Slice preview(const Limits &limits,
                      uint32_t maximumNominalTicks) const {
            return previewImpl(limits, maximumNominalTicks, false);
        }

        // Builds a smooth terminal tail when the producer has no future
        // waypoint. It decelerates once and then holds the current position.
        Slice previewHold(const Limits &limits, uint32_t nominalTicks) const {
            return previewImpl(limits, nominalTicks, true);
        }

        void commit(const Slice &slice, uint32_t actualDurationTicks) {
            state_ = slice.next;
            state_.timingCarryTicks = slice.next.timingCarryTicks +
                                      slice.nominalTicks - actualDurationTicks;
            if (slice.finishesWaypoint && hasWaypoint()) {
                for (size_t index = 1; index < waypointCount_; ++index)
                    waypoints_[index - 1] = waypoints_[index];
                --waypointCount_;
                elapsedWaypointTicks_ = 0;
            } else if (!slice.holdSlice && hasWaypoint()) {
                elapsedWaypointTicks_ += slice.nominalTicks;
            }
        }

      private:
        Slice previewImpl(const Limits &limits, uint32_t maximumNominalTicks,
                          bool hold) const {
            Slice result{};
            result.next = state_;
            result.holdSlice = hold;
            if ((!hasWaypoint() && !hold) || maximumNominalTicks == 0 ||
                ticksPerSecond_ == 0) {
                return result;
            }

            result.nominalTicks =
                hold ? maximumNominalTicks
                     : static_cast<uint32_t>(std::min<uint64_t>(
                           remainingWaypointTicks(), maximumNominalTicks));
            result.remainingWaypointTicks =
                hold ? 0 : remainingWaypointTicks() - result.nominalTicks;
            result.finishesWaypoint =
                !hold && result.remainingWaypointTicks == 0;

            int64_t requested = static_cast<int64_t>(result.nominalTicks) +
                                state_.timingCarryTicks;
            requested = std::max<int64_t>(1, requested);
            requested = std::min<int64_t>(requested,
                                          std::numeric_limits<uint32_t>::max());
            result.requestedTicks = static_cast<uint32_t>(requested);

            const double dt = ticksToSeconds(result.requestedTicks);
            const double speedLimit = std::max(0.0, limits.speedStepsPerSecond);
            const double accelerationLimit =
                std::max(0.0, limits.accelerationStepsPerSecondSquared);
            if (speedLimit <= 0.0 || accelerationLimit <= 0.0 || dt <= 0.0) {
                result.next.velocityStepsPerSecond = 0.0;
                result.next.accelerationStepsPerSecondSquared = 0.0;
                return result;
            }

            const double innerMinimum =
                static_cast<double>(range_.minimumSteps + range_.guardSteps);
            const double innerMaximum =
                static_cast<double>(range_.maximumSteps - range_.guardSteps);
            const double position = state_.continuousPositionSteps;
            const double target =
                hold ? position
                     : clamp(static_cast<double>(waypoints_[0].positionSteps),
                             innerMinimum, innerMaximum);
            const double remainingSeconds =
                hold ? dt
                     : ticksToSeconds(std::max<uint64_t>(
                           result.nominalTicks, remainingWaypointTicks()));
            const double error = target - position;
            result.referenceErrorSteps = error;

            // Preserve the smooth July 20 16:05 follower: once a waypoint has
            // less than one jerk-ramp remaining, carry its residual forward
            // instead of demanding an increasingly large velocity correction
            // at the BLE packet cadence.
            const double correctionSeconds =
                hold ? dt
                     : std::max(remainingSeconds,
                                kJerkRampMilliseconds / 1000.0);
            double desiredVelocity = hold ? 0.0 : error / correctionSeconds;
            desiredVelocity = clamp(desiredVelocity, -speedLimit, speedLimit);

            // The stopping-distance envelope is normally inactive. It only
            // removes outward velocity close enough to an interior boundary
            // that the configured acceleration could not stop in time.
            const double upperDistance = std::max(0.0, innerMaximum - position);
            const double lowerDistance = std::max(0.0, position - innerMinimum);
            const double upperEnvelope = std::min(
                speedLimit, std::sqrt(2.0 * accelerationLimit * upperDistance));
            const double lowerEnvelope = std::min(
                speedLimit, std::sqrt(2.0 * accelerationLimit * lowerDistance));
            const double unboundedVelocity = desiredVelocity;
            desiredVelocity =
                clamp(desiredVelocity, -lowerEnvelope, upperEnvelope);
            result.boundaryEnvelopeActive =
                desiredVelocity != unboundedVelocity;

            double desiredAcceleration =
                (desiredVelocity - state_.velocityStepsPerSecond) / dt;
            desiredAcceleration = clamp(desiredAcceleration, -accelerationLimit,
                                        accelerationLimit);
            const double jerkLimit =
                accelerationLimit / (kJerkRampMilliseconds / 1000.0);
            double nextAcceleration =
                clamp(moveToward(state_.accelerationStepsPerSecondSquared,
                                 desiredAcceleration, jerkLimit * dt),
                      -accelerationLimit, accelerationLimit);
            double nextVelocity =
                clamp(state_.velocityStepsPerSecond + nextAcceleration * dt,
                      -speedLimit, speedLimit);

            // A hold is terminal: do not let braking acceleration create a
            // tiny reverse motion while its jerk-limited value returns to zero.
            if (hold && state_.velocityStepsPerSecond * nextVelocity <= 0.0) {
                nextVelocity = 0.0;
                nextAcceleration =
                    moveToward(state_.accelerationStepsPerSecondSquared, 0.0,
                               jerkLimit * dt);
            }

            const double continuousDelta =
                (state_.velocityStepsPerSecond + nextVelocity) * 0.5 * dt;
            const double desiredContinuousPosition = position + continuousDelta;
            double boundedContinuousPosition = desiredContinuousPosition;
            if (position >= range_.minimumSteps &&
                position <= range_.maximumSteps) {
                boundedContinuousPosition =
                    clamp(desiredContinuousPosition,
                          static_cast<double>(range_.minimumSteps),
                          static_cast<double>(range_.maximumSteps));
            } else if (position < range_.minimumSteps) {
                boundedContinuousPosition =
                    std::max(desiredContinuousPosition, position);
            } else {
                boundedContinuousPosition =
                    std::min(desiredContinuousPosition, position);
            }
            result.safetyClampActive =
                boundedContinuousPosition != desiredContinuousPosition;

            const double stepAccumulator =
                boundedContinuousPosition - position + state_.fractionalSteps;
            long roundedSteps = std::lround(stepAccumulator);
            const int64_t minimumAllowed =
                static_cast<int64_t>(range_.minimumSteps) -
                state_.positionSteps;
            const int64_t maximumAllowed =
                static_cast<int64_t>(range_.maximumSteps) -
                state_.positionSteps;
            if (state_.positionSteps >= range_.minimumSteps &&
                state_.positionSteps <= range_.maximumSteps) {
                roundedSteps = static_cast<long>(std::max<int64_t>(
                    minimumAllowed,
                    std::min<int64_t>(maximumAllowed, roundedSteps)));
            } else if (state_.positionSteps < range_.minimumSteps) {
                roundedSteps = std::max<long>(0, roundedSteps);
            } else {
                roundedSteps = std::min<long>(0, roundedSteps);
            }
            roundedSteps = std::max<long>(
                std::numeric_limits<int16_t>::min(),
                std::min<long>(std::numeric_limits<int16_t>::max(),
                               roundedSteps));

            result.steps = static_cast<int16_t>(roundedSteps);
            result.next.positionSteps += result.steps;
            result.next.continuousPositionSteps = boundedContinuousPosition;
            result.next.velocityStepsPerSecond = nextVelocity;
            result.next.accelerationStepsPerSecondSquared = nextAcceleration;
            result.next.fractionalSteps = stepAccumulator - result.steps;
            if (result.safetyClampActive) {
                result.next.velocityStepsPerSecond = 0.0;
                result.next.accelerationStepsPerSecondSquared = 0.0;
            }
            result.reference = {target, desiredVelocity, desiredAcceleration};
            result.minimumRangeMarginSteps = std::min(
                result.next.continuousPositionSteps - range_.minimumSteps,
                range_.maximumSteps - result.next.continuousPositionSteps);
            return result;
        }

        double ticksToSeconds(uint64_t ticks) const {
            return static_cast<double>(ticks) /
                   static_cast<double>(ticksPerSecond_);
        }

        static int32_t clampInt(int32_t value, int32_t low, int32_t high) {
            return std::max(low, std::min(high, value));
        }
        static double clamp(double value, double low, double high) {
            return std::max(low, std::min(high, value));
        }
        static double moveToward(double current, double target,
                                 double maximumDelta) {
            if (target > current)
                return std::min(target, current + maximumDelta);
            return std::max(target, current - maximumDelta);
        }

        uint32_t ticksPerSecond_;
        State state_{};
        Range range_{};
        std::array<Waypoint, kWaypointCapacity> waypoints_{};
        size_t waypointCount_ = 0;
        uint64_t elapsedWaypointTicks_ = 0;
    };

}  // namespace timed_streaming
