#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

#include "timed_streaming_tuning.h"

namespace timed_streaming {

    constexpr uint32_t kSliceMilliseconds = 4;
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
        int8_t pendingVelocityDirection = 0;
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
        bool edgeRepulsionActive = false;
        bool inwardRecoveryActive = false;
        bool safetyClampActive = false;
        double referenceErrorSteps = 0.0;
        double minimumRangeMarginSteps = 0.0;
    };

    inline double jerkAwareVelocityEnvelope(
        double distanceSteps, double accelerationStepsPerSecondSquared,
        double jerkRampSeconds, double speedLimitStepsPerSecond) {
        if (distanceSteps <= 0.0 ||
            accelerationStepsPerSecondSquared <= 0.0 ||
            speedLimitStepsPerSecond <= 0.0)
            return 0.0;
        const double acceleration = accelerationStepsPerSecondSquared;
        const double ramp = std::max(0.0, jerkRampSeconds);
        // Conservatively reserve one complete jerk-ramp interval before full
        // braking. Solving d = v*t + v^2/(2*a) produces an envelope that
        // continuously reaches zero at the boundary.
        const double velocity = acceleration *
            (std::sqrt(ramp * ramp + 2.0 * distanceSteps / acceleration) -
             ramp);
        return std::max(0.0,
                        std::min(speedLimitStepsPerSecond, velocity));
    }

    inline double edgeRepulsionAcceleration(
        double positionSteps, double innerMinimumSteps,
        double innerMaximumSteps,
        double accelerationLimitStepsPerSecondSquared, double strength) {
        const double span = innerMaximumSteps - innerMinimumSteps;
        if (span <= 0.0 || accelerationLimitStepsPerSecondSquared <= 0.0 ||
            strength <= 0.0)
            return 0.0;
        // Begin the soft field inside the outer 15% of the guarded span. The
        // separate stopping envelope can engage earlier when dynamics demand
        // it, while ordinary mid-range tracking remains undistorted.
        const double influenceDistance = std::max(1.0, span * 0.15);
        const auto magnitude = [&](double distance) {
            const double proximity = std::max(
                0.0, std::min(1.0,
                              1.0 - distance / influenceDistance));
            return accelerationLimitStepsPerSecondSquared *
                   (1.0 - std::exp(-3.0 * strength * proximity * proximity));
        };
        const double lowerDistance = positionSteps - innerMinimumSteps;
        const double upperDistance = innerMaximumSteps - positionSteps;
        return std::max(
            -accelerationLimitStepsPerSecondSquared,
            std::min(accelerationLimitStepsPerSecondSquared,
                     magnitude(lowerDistance) - magnitude(upperDistance)));
    }

    struct DropoutMassState {
        double positionSteps = 0.0;
        double velocityStepsPerSecond = 0.0;
    };

    inline DropoutMassState advanceDropoutMass(
        DropoutMassState state, double originSteps,
        double initialVelocityStepsPerSecond, double innerMinimumSteps,
        double innerMaximumSteps, const Limits &limits,
        const TuningParameters &parameters, double dtSeconds) {
        const double span =
            std::max(1.0, innerMaximumSteps - innerMinimumSteps);
        const double center =
            (innerMinimumSteps + innerMaximumSteps) * 0.5;
        const double halfSpan = span * 0.5;
        const double dt = std::max(0.0, dtSeconds);
        if (dt == 0.0) return state;
        const double decaySeconds = std::max(
            dt, parameters.momentumDecayMilliseconds / 1000.0);
        const double dragAcceleration =
            -state.velocityStepsPerSecond / decaySeconds;
        const double springAcceleration =
            parameters.centerSpringStrength *
            limits.accelerationStepsPerSecondSquared *
            (center - state.positionSteps) / halfSpan;
        const double barrierAcceleration = edgeRepulsionAcceleration(
            state.positionSteps, innerMinimumSteps, innerMaximumSteps,
            limits.accelerationStepsPerSecondSquared,
            parameters.edgeRepulsionStrength);
        const double acceleration = std::max(
            -limits.accelerationStepsPerSecondSquared,
            std::min(limits.accelerationStepsPerSecondSquared,
                     dragAcceleration + springAcceleration +
                         barrierAcceleration));
        state.velocityStepsPerSecond = std::max(
            -limits.speedStepsPerSecond,
            std::min(limits.speedStepsPerSecond,
                     state.velocityStepsPerSecond + acceleration * dt));
        state.positionSteps += state.velocityStepsPerSecond * dt;

        const double maximumCoastSteps =
            span * parameters.maximumCoastFraction;
        const double displacement = state.positionSteps - originSteps;
        if (initialVelocityStepsPerSecond * displacement > 0.0 &&
            std::abs(displacement) > maximumCoastSteps) {
            state.positionSteps = originSteps +
                std::copysign(maximumCoastSteps, displacement);
            if (initialVelocityStepsPerSecond *
                    state.velocityStepsPerSecond >
                0.0)
                state.velocityStepsPerSecond = 0.0;
        }
        if (state.positionSteps <= innerMinimumSteps) {
            state.positionSteps = innerMinimumSteps;
            if (state.velocityStepsPerSecond < 0.0)
                state.velocityStepsPerSecond = 0.0;
        } else if (state.positionSteps >= innerMaximumSteps) {
            state.positionSteps = innerMaximumSteps;
            if (state.velocityStepsPerSecond > 0.0)
                state.velocityStepsPerSecond = 0.0;
        }
        return state;
    }

    // Sequential transactional follower for FastAccelStepper::moveTimed().
    // A preview never mutates state, so driver retries cannot duplicate motion
    // or consume reference time.
    class Planner {
      public:
        explicit Planner(uint32_t ticksPerSecond,
                         uint32_t jerkRampMilliseconds = 200)
            : ticksPerSecond_(ticksPerSecond),
              jerkRampMilliseconds_(jerkRampMilliseconds) {}

        void setJerkRampMilliseconds(uint32_t value) {
            jerkRampMilliseconds_ = std::max<uint32_t>(1, value);
        }

        void setMomentumDecayMilliseconds(uint32_t value) {
            momentumDecayMilliseconds_ = std::max<uint32_t>(1, value);
        }

        void setEdgeRepulsionStrength(double value) {
            edgeRepulsionStrength_ = std::max(0.0, value);
        }

        uint32_t jerkRampMilliseconds() const {
            return jerkRampMilliseconds_;
        }

        void reset(int32_t positionSteps) {
            state_ = {};
            state_.positionSteps = positionSteps;
            state_.continuousPositionSteps = positionSteps;
            referenceStartPositionSteps_ = positionSteps;
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
            referenceStartPositionSteps_ = actualPositionSteps;
            elapsedWaypointTicks_ = 0;
        }

        Reference referenceAt(uint64_t) const {
            if (!hasWaypoint())
                return {state_.continuousPositionSteps, 0.0, 0.0};
            const double durationTicks =
                static_cast<double>(waypoints_[0].durationTicks);
            const double progress =
                durationTicks > 0
                    ? std::min(1.0, elapsedWaypointTicks_ / durationTicks)
                    : 1.0;
            const double distance =
                waypoints_[0].positionSteps - referenceStartPositionSteps_;
            return {
                referenceStartPositionSteps_ + distance * progress,
                distance /
                    ticksToSeconds(
                        std::max<uint64_t>(1, waypoints_[0].durationTicks)),
                0.0};
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
                referenceStartPositionSteps_ =
                    waypoints_[0].positionSteps;
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
            double referencePosition = position;
            double feedforwardVelocity = 0.0;
            if (!hold) {
                const double durationTicks =
                    static_cast<double>(waypoints_[0].durationTicks);
                const double endProgress =
                    durationTicks > 0
                        ? std::min(
                              1.0,
                              (elapsedWaypointTicks_ + result.nominalTicks) /
                                  durationTicks)
                        : 1.0;
                const double referenceDistance =
                    target - referenceStartPositionSteps_;
                referencePosition =
                    referenceStartPositionSteps_ +
                    referenceDistance * endProgress;
                feedforwardVelocity =
                    referenceDistance /
                    ticksToSeconds(std::max<uint64_t>(
                        1, waypoints_[0].durationTicks));
            }
            result.referenceErrorSteps = referencePosition - position;

            // Streaming intentionally follows reference velocity rather than
            // closing a proportional position loop on every short BLE segment.
            // Lag and translation are fitted by the host analysis; firmware
            // only applies feed-forward, jerk/acceleration limits, and the hard
            // stopping envelope.
            double requestedVelocity = hold ? 0.0 : feedforwardVelocity;
            requestedVelocity =
                clamp(requestedVelocity, -speedLimit, speedLimit);

            // Position is open-loop, but it can still begin outside the
            // guarded play range after a re-home, frame transition, or a
            // safety stop. In that state the request is subordinate to a
            // smooth inward restoring force. Acceleration and jerk limiting
            // below make the recovery elastic; the hard clamp remains the
            // final protection against any additional outward step.
            const bool belowInnerRange = position < innerMinimum;
            const bool aboveInnerRange = position > innerMaximum;
            double desiredVelocity = requestedVelocity;
            if (belowInnerRange || aboveInnerRange) {
                const double penetration =
                    belowInnerRange ? innerMinimum - position
                                    : position - innerMaximum;
                const double recoverySpeed = std::min(
                    speedLimit,
                    std::sqrt(2.0 * accelerationLimit * penetration));
                desiredVelocity = belowInnerRange ? recoverySpeed
                                                  : -recoverySpeed;
                result.inwardRecoveryActive = true;
            } else if (!hold) {
                // A direction request cannot flip the virtual mass. Latch the
                // reversal, dissipate the old momentum, and only then release
                // motion in the newly requested direction. Persisting the
                // latch in planner state prevents alternating short BLE
                // segments from chattering around zero velocity.
                int8_t pendingDirection =
                    state_.pendingVelocityDirection;
                if (pendingDirection == 0 &&
                    state_.velocityStepsPerSecond * requestedVelocity < 0.0) {
                    pendingDirection =
                        requestedVelocity > 0.0 ? 1 : -1;
                }
                if (pendingDirection != 0) {
                    const double releaseVelocity =
                        std::max(0.5, speedLimit * 0.04);
                    if (std::abs(state_.velocityStepsPerSecond) >
                        releaseVelocity) {
                        const double momentumSeconds =
                            momentumDecayMilliseconds_ / 1000.0;
                        desiredVelocity =
                            state_.velocityStepsPerSecond *
                            std::exp(-dt /
                                     std::max(dt, momentumSeconds));
                    } else {
                        pendingDirection = 0;
                        desiredVelocity = requestedVelocity;
                    }
                }
                result.next.pendingVelocityDirection = pendingDirection;
            }

            // The stopping-distance envelope is normally inactive. It only
            // removes outward velocity close enough to an interior boundary
            // that the configured acceleration could not stop in time.
            const double upperDistance = std::max(0.0, innerMaximum - position);
            const double lowerDistance = std::max(0.0, position - innerMinimum);
            const double jerkRampSeconds =
                jerkRampMilliseconds_ / 1000.0;
            const double upperEnvelope = jerkAwareVelocityEnvelope(
                upperDistance, accelerationLimit, jerkRampSeconds,
                speedLimit);
            const double lowerEnvelope = jerkAwareVelocityEnvelope(
                lowerDistance, accelerationLimit, jerkRampSeconds,
                speedLimit);
            const double unboundedVelocity = desiredVelocity;
            desiredVelocity =
                clamp(desiredVelocity, -lowerEnvelope, upperEnvelope);
            const double repulsionAcceleration =
                edgeRepulsionAcceleration(
                    position, innerMinimum, innerMaximum, accelerationLimit,
                    edgeRepulsionStrength_);
            result.edgeRepulsionActive =
                std::abs(repulsionAcceleration) > 1e-9;
            if (result.edgeRepulsionActive && !result.inwardRecoveryActive) {
                // Convert the potential gradient into a single-valued inward
                // velocity field. Giving the field directional priority
                // prevents a persistent outward request from fighting it on
                // alternating 4 ms slices and producing edge chatter.
                const double repulsionVelocity =
                    std::copysign(
                        speedLimit * 0.25 *
                            std::abs(repulsionAcceleration) /
                            accelerationLimit,
                        repulsionAcceleration);
                desiredVelocity = repulsionAcceleration > 0.0
                    ? std::max(desiredVelocity, repulsionVelocity)
                    : std::min(desiredVelocity, repulsionVelocity);
                desiredVelocity =
                    clamp(desiredVelocity, -lowerEnvelope, upperEnvelope);
            }
            result.boundaryEnvelopeActive = result.inwardRecoveryActive ||
                result.edgeRepulsionActive ||
                desiredVelocity != unboundedVelocity;

            double desiredAcceleration =
                (desiredVelocity - state_.velocityStepsPerSecond) / dt;
            desiredAcceleration = clamp(desiredAcceleration, -accelerationLimit,
                                        accelerationLimit);
            const double jerkLimit =
                accelerationLimit / (jerkRampMilliseconds_ / 1000.0);
            double nextAcceleration =
                clamp(moveToward(state_.accelerationStepsPerSecondSquared,
                                 desiredAcceleration, jerkLimit * dt),
                      -accelerationLimit, accelerationLimit);
            double nextVelocity =
                clamp(state_.velocityStepsPerSecond + nextAcceleration * dt,
                      -speedLimit, speedLimit);
            if (!belowInnerRange && !aboveInnerRange) {
                // The force field supplies the smooth behavior. This
                // independent velocity barrier is the non-tunable guarantee:
                // no outward state can retain speed that cannot stop before
                // the guarded play-range edge.
                nextVelocity = clamp(nextVelocity, -lowerEnvelope,
                                     upperEnvelope);
            }

            // A hold is terminal: do not let braking acceleration create a
            // tiny reverse motion while its jerk-limited value returns to zero.
            if (hold && !result.inwardRecoveryActive &&
                state_.velocityStepsPerSecond * nextVelocity <= 0.0) {
                nextVelocity = 0.0;
                nextAcceleration =
                    moveToward(state_.accelerationStepsPerSecondSquared, 0.0,
                               jerkLimit * dt);
            }

            const double continuousDelta =
                (state_.velocityStepsPerSecond + nextVelocity) * 0.5 * dt;
            const double desiredContinuousPosition = position + continuousDelta;
            double boundedContinuousPosition = desiredContinuousPosition;
            if (position >= innerMinimum && position <= innerMaximum) {
                boundedContinuousPosition =
                    clamp(desiredContinuousPosition,
                          innerMinimum, innerMaximum);
            } else if (position < innerMinimum) {
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
                static_cast<int64_t>(range_.minimumSteps + range_.guardSteps) -
                state_.positionSteps;
            const int64_t maximumAllowed =
                static_cast<int64_t>(range_.maximumSteps - range_.guardSteps) -
                state_.positionSteps;
            if (state_.positionSteps >=
                    range_.minimumSteps + range_.guardSteps &&
                state_.positionSteps <=
                    range_.maximumSteps - range_.guardSteps) {
                roundedSteps = static_cast<long>(std::max<int64_t>(
                    minimumAllowed,
                    std::min<int64_t>(maximumAllowed, roundedSteps)));
            } else if (state_.positionSteps <
                       range_.minimumSteps + range_.guardSteps) {
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
            result.reference = {
                referencePosition, feedforwardVelocity, 0.0};
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
        uint32_t jerkRampMilliseconds_ = 200;
        uint32_t momentumDecayMilliseconds_ = 200;
        double edgeRepulsionStrength_ = 2.0;
        State state_{};
        Range range_{};
        std::array<Waypoint, kWaypointCapacity> waypoints_{};
        size_t waypointCount_ = 0;
        uint64_t elapsedWaypointTicks_ = 0;
        double referenceStartPositionSteps_ = 0.0;
    };

}  // namespace timed_streaming
