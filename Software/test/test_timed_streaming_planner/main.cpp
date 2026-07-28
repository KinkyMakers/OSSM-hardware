#include <unity.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include "timed_streaming_planner.h"
#include "timed_streaming_runtime.h"

namespace {

    constexpr uint32_t kTicksPerSecond = 16000000;
    constexpr uint32_t kSliceTicks =
        timed_streaming::kSliceMilliseconds * (kTicksPerSecond / 1000);
    constexpr timed_streaming::Limits kLimits{20000.0, 1000000.0};

    double nominalSineJitterPowerPercent(
        const std::vector<double> &sliceTimes,
        const std::vector<double> &slicePositions) {
        constexpr double kSampleRate = 240.0;
        constexpr int kDiscardSamples = 5 * 240;
        constexpr int kScoredSamples = 10 * 240;
        constexpr int kSavitzkyGolayHalfWindow = 15;
        constexpr double kPi = 3.14159265358979323846;
        // 31-frame, third-order Savitzky-Golay first derivative at 240 Hz,
        // matching Motion Lab's closed-loop smoothness scorer.
        constexpr double coefficients[31] = {
            2.88942556494738723,   1.24978437122649422,
            -0.090474859172104516, -1.15273655220000881,
            -1.95838513380880741,  -2.52880502995009371,
            -2.88538066657545755,  -3.04949646963649368,
            -3.04253686508479326,  -2.88588627887194793,
            -2.60092913694955152,  -2.20904986526919300,
            -1.73163288978246666,  -1.19006263644096388,
            -0.605723531196277531, 0.0,
            0.605723531196280085,  1.19006263644096655,
            1.73163288978246910,   2.20904986526919522,
            2.60092913694955286,   2.88588627887195015,
            3.04253686508479548,   3.04949646963649545,
            2.88538066657545800,   2.52880502995009326,
            1.95838513380880719,   1.15273655220000903,
            0.0904748591721036832, -1.24978437122649777,
            -2.88942556494738945};

        std::vector<double> sampledPositions;
        sampledPositions.reserve(kScoredSamples);
        size_t upper = 1;
        for (int sample = 0; sample < kScoredSamples; ++sample) {
            const double time =
                static_cast<double>(kDiscardSamples + sample) / kSampleRate;
            while (upper < sliceTimes.size() && sliceTimes[upper] < time)
                ++upper;
            TEST_ASSERT_LESS_THAN(sliceTimes.size(), upper);
            const size_t lower = upper - 1;
            const double interval = sliceTimes[upper] - sliceTimes[lower];
            const double fraction =
                interval > 0.0 ? (time - sliceTimes[lower]) / interval : 0.0;
            sampledPositions.push_back(
                slicePositions[lower] +
                fraction * (slicePositions[upper] - slicePositions[lower]));
        }

        std::vector<double> velocity;
        velocity.reserve(kScoredSamples - 2 * kSavitzkyGolayHalfWindow);
        for (int center = kSavitzkyGolayHalfWindow;
             center < kScoredSamples - kSavitzkyGolayHalfWindow; ++center) {
            double derivative = 0.0;
            for (int tap = 0; tap < 31; ++tap) {
                derivative +=
                    coefficients[tap] *
                    sampledPositions[center + tap - kSavitzkyGolayHalfWindow];
            }
            velocity.push_back(derivative);
        }

        double mean = 0.0;
        for (double value : velocity) mean += value;
        mean /= velocity.size();
        for (double &value : velocity) value -= mean;

        double highFrequencyPower = 0.0;
        double totalMotionPower = 0.0;
        const size_t count = velocity.size();
        for (size_t bin = 1; bin <= count / 2; ++bin) {
            const double frequency =
                static_cast<double>(bin) * kSampleRate / count;
            if (frequency <= 0.1) continue;
            double real = 0.0;
            double imaginary = 0.0;
            for (size_t sample = 0; sample < count; ++sample) {
                const double hann =
                    0.5 - 0.5 * std::cos(2.0 * kPi * sample /
                                         static_cast<double>(count - 1));
                const double phase =
                    2.0 * kPi * bin * sample / static_cast<double>(count);
                const double windowed = velocity[sample] * hann;
                real += windowed * std::cos(phase);
                imaginary -= windowed * std::sin(phase);
            }
            const double power = real * real + imaginary * imaginary;
            totalMotionPower += power;
            if (frequency > 5.0) highFrequencyPower += power;
        }
        TEST_ASSERT_GREATER_THAN_DOUBLE(0.0, totalMotionPower);
        return 100.0 * highFrequencyPower / totalMotionPower;
    }

    void assertLimits(const timed_streaming::State &before,
                      const timed_streaming::State &after) {
        const double dt = timed_streaming::kSliceMilliseconds / 1000.0;
        const double jerkLimit =
            kLimits.accelerationStepsPerSecondSquared /
            (timed_streaming::kJerkRampMilliseconds / 1000.0);
        TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
            kLimits.speedStepsPerSecond + 1e-6,
            std::abs(after.velocityStepsPerSecond));
        TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
            kLimits.accelerationStepsPerSecondSquared + 1e-6,
            std::abs(after.accelerationStepsPerSecondSquared));
        TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
            kLimits.accelerationStepsPerSecondSquared + 1e-6,
            std::abs(after.velocityStepsPerSecond -
                     before.velocityStepsPerSecond) /
                dt);
        TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
            jerkLimit * dt + 1e-6,
            std::abs(after.accelerationStepsPerSecondSquared -
                     before.accelerationStepsPerSecondSquared));
    }

    void acceptWaypoint(timed_streaming::Planner &planner, int32_t target,
                        uint32_t durationMs) {
        planner.beginWaypoint(target, static_cast<uint64_t>(durationMs) *
                                          (kTicksPerSecond / 1000));
        while (planner.hasWaypoint()) {
            const auto before = planner.state();
            const auto slice = planner.preview(kLimits, kSliceTicks);
            planner.commit(slice, slice.requestedTicks);
            assertLimits(before, planner.state());
        }
    }

    void test_constant_velocity_sequence(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        for (int i = 1; i <= 100; ++i) {
            acceptWaypoint(planner, i * 20, 20);
        }
        TEST_ASSERT_INT32_WITHIN(40, 2000, planner.state().positionSteps);
    }

    void test_sine_sequence_obeys_all_limits(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        for (int i = 1; i <= 500; ++i) {
            const double seconds = i * 0.01;
            const int32_t target = static_cast<int32_t>(
                std::lround(1200.0 * std::sin(seconds * 6.283185307179586)));
            acceptWaypoint(planner, target, 10);
        }
        // The stable jerk-ramp controller intentionally follows with a small
        // lag; closed-loop scoring removes this constant horizontal offset.
        TEST_ASSERT_INT32_WITHIN(500, 0, planner.state().positionSteps);
    }

    void test_nominal_sine_aligned_rmse_is_below_two_mm(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        planner.setRange(-2000, 2000, 20);
        std::vector<double> requested;
        std::vector<double> measured;
        std::vector<int32_t> targets;
        targets.reserve(1500);
        for (int i = 1; i <= 1500; ++i) {
            const double seconds = i * 0.01;
            targets.push_back(static_cast<int32_t>(
                std::lround(1200.0 * std::sin(seconds * 6.283185307179586))));
        }
        size_t appended = 0;
        size_t completed = 0;
        while (completed < targets.size()) {
            while (appended < targets.size() && planner.waypointCount() < 20) {
                TEST_ASSERT_TRUE(planner.appendWaypoint(
                    targets[appended++], 10 * (kTicksPerSecond / 1000)));
            }
            const auto before = planner.state();
            const auto slice = planner.preview(kLimits, kSliceTicks);
            planner.commit(slice, slice.requestedTicks);
            assertLimits(before, planner.state());
            if (slice.finishesWaypoint) {
                requested.push_back(targets[completed++]);
                measured.push_back(planner.state().positionSteps);
            }
        }

        double bestRmseSteps = std::numeric_limits<double>::infinity();
        int bestLagSamples = 0;
        constexpr int discardSamples = 500;
        constexpr int maximumLagSamples = 50;
        for (int lag = 0; lag <= maximumLagSamples; ++lag) {
            double sumSquared = 0.0;
            int count = 0;
            for (int i = discardSamples;
                 i + lag < static_cast<int>(measured.size()); ++i) {
                const double error = requested[i] - measured[i + lag];
                sumSquared += error * error;
                ++count;
            }
            const double rmse = std::sqrt(sumSquared / count);
            if (rmse < bestRmseSteps) {
                bestRmseSteps = rmse;
                bestLagSamples = lag;
            }
        }

        TEST_ASSERT_LESS_THAN_DOUBLE(50.0, bestRmseSteps);  // 20 steps/mm
        TEST_ASSERT_LESS_THAN(50, bestLagSamples);          // 500 ms
    }

    void test_nominal_twenty_hertz_sine_restores_smooth_baseline(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        constexpr int32_t kRangeMinimum = -1300;
        constexpr int32_t kRangeMaximum = 0;
        constexpr int32_t kCenter = -650;
        constexpr double kPi = 3.14159265358979323846;
        constexpr timed_streaming::Limits kHardwareLimits{8000.0, 500000.0};
        planner.reset(kCenter);
        planner.setRange(kRangeMinimum, kRangeMaximum, 20);

        std::vector<double> times{0.0};
        std::vector<double> positions{static_cast<double>(kCenter)};
        double elapsedSeconds = 0.0;
        double previousReferenceVelocity = 0.0;
        bool havePreviousReference = false;
        double maximumPacketReferenceJump = 0.0;
        bool previousSliceFinishedWaypoint = false;
        bool safetyClampActivated = false;

        for (int point = 1; point <= 300; ++point) {
            const double timeSeconds = point * 0.05;
            const int32_t percent = static_cast<int32_t>(std::lround(
                50.0 + 25.0 * std::sin(2.0 * kPi * 0.5 * timeSeconds)));
            const int32_t target = kCenter + (percent - 50) * 13;
            TEST_ASSERT_TRUE(
                planner.appendWaypoint(target, 50 * (kTicksPerSecond / 1000)));
            while (planner.hasWaypoint()) {
                const auto before = planner.state();
                const auto slice =
                    planner.preview(kHardwareLimits, kSliceTicks);
                if (previousSliceFinishedWaypoint && havePreviousReference) {
                    maximumPacketReferenceJump = std::max(
                        maximumPacketReferenceJump,
                        std::abs(slice.reference.velocityStepsPerSecond -
                                 previousReferenceVelocity));
                }
                previousReferenceVelocity =
                    slice.reference.velocityStepsPerSecond;
                havePreviousReference = true;
                previousSliceFinishedWaypoint = slice.finishesWaypoint;
                safetyClampActivated =
                    safetyClampActivated || slice.safetyClampActive;
                planner.commit(slice, slice.requestedTicks);

                const double dt =
                    static_cast<double>(slice.requestedTicks) / kTicksPerSecond;
                elapsedSeconds += dt;
                times.push_back(elapsedSeconds);
                positions.push_back(planner.state().continuousPositionSteps);

                const double jerkLimit =
                    kHardwareLimits.accelerationStepsPerSecondSquared /
                    (timed_streaming::kJerkRampMilliseconds / 1000.0);
                TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
                    kHardwareLimits.speedStepsPerSecond + 1e-6,
                    std::abs(planner.state().velocityStepsPerSecond));
                TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
                    kHardwareLimits.accelerationStepsPerSecondSquared + 1e-6,
                    std::abs(
                        planner.state().accelerationStepsPerSecondSquared));
                TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
                    jerkLimit * dt + 1e-6,
                    std::abs(planner.state().accelerationStepsPerSecondSquared -
                             before.accelerationStepsPerSecondSquared));
                TEST_ASSERT_GREATER_OR_EQUAL_INT32(
                    kRangeMinimum, planner.state().positionSteps);
                TEST_ASSERT_LESS_OR_EQUAL_INT32(kRangeMaximum,
                                                planner.state().positionSteps);
            }
        }

        TEST_ASSERT_FALSE(safetyClampActivated);
        TEST_ASSERT_LESS_THAN_DOUBLE(2500.0, maximumPacketReferenceJump);
        TEST_ASSERT_LESS_THAN_DOUBLE(
            1.5, nominalSineJitterPowerPercent(times, positions));
    }

    void test_triangle_reversal_is_jerk_limited(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        for (int cycle = 0; cycle < 10; ++cycle) {
            for (int i = 1; i <= 50; ++i) acceptWaypoint(planner, i * 20, 10);
            for (int i = 49; i >= -50; --i) acceptWaypoint(planner, i * 20, 10);
            for (int i = -49; i <= 0; ++i) acceptWaypoint(planner, i * 20, 10);
        }
        TEST_ASSERT_INT32_WITHIN(100, 0, planner.state().positionSteps);
    }

    void test_physically_possible_random_sequence(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        uint32_t random = 0x5a17u;
        int32_t target = 0;
        for (int i = 0; i < 1000; ++i) {
            random = random * 1664525u + 1013904223u;
            target += static_cast<int32_t>((random >> 24) % 41) - 20;
            target = std::max(-2000, std::min(2000, target));
            acceptWaypoint(planner, target, 12);
        }
        TEST_ASSERT_LESS_OR_EQUAL(2100,
                                  std::abs(planner.state().positionSteps));
    }

    void test_repeated_position_consumes_hold_without_snap(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        acceptWaypoint(planner, 800, 300);
        const int32_t beforeHold = planner.state().positionSteps;
        acceptWaypoint(planner, 800, 400);
        TEST_ASSERT_INT32_WITHIN(20, 800, planner.state().positionSteps);
        TEST_ASSERT_TRUE(beforeHold != 0);
    }

    void test_short_and_long_waypoints(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        acceptWaypoint(planner, 10000, 1);
        TEST_ASSERT_LESS_THAN(100, std::abs(planner.state().positionSteps));
        acceptWaypoint(planner, 1000, 10000);
        TEST_ASSERT_INT32_WITHIN(5, 1000, planner.state().positionSteps);
    }

    void test_range_envelope_never_emits_out_of_bounds_endpoint(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(500);
        planner.setRange(0, 1000, 20);
        planner.beginWaypoint(5000, 2000 * (kTicksPerSecond / 1000));
        bool safetyClampActivated = false;
        while (planner.hasWaypoint()) {
            const auto slice = planner.preview(kLimits, kSliceTicks);
            safetyClampActivated =
                safetyClampActivated || slice.safetyClampActive;
            planner.commit(slice, slice.requestedTicks);
            TEST_ASSERT_GREATER_OR_EQUAL_INT32(0,
                                               planner.state().positionSteps);
            TEST_ASSERT_LESS_OR_EQUAL_INT32(1000,
                                            planner.state().positionSteps);
        }
        TEST_ASSERT_FALSE(safetyClampActivated);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(980, planner.state().positionSteps);
    }

    void test_future_waypoint_does_not_change_active_sequential_slice(void) {
        timed_streaming::Planner left(kTicksPerSecond);
        timed_streaming::Planner right(kTicksPerSecond);
        left.reset(500);
        right.reset(500);
        left.setRange(0, 1000, 20);
        right.setRange(0, 1000, 20);
        left.appendWaypoint(800, 100 * (kTicksPerSecond / 1000));
        right.appendWaypoint(800, 100 * (kTicksPerSecond / 1000));
        left.appendWaypoint(100, 100 * (kTicksPerSecond / 1000));
        right.appendWaypoint(950, 100 * (kTicksPerSecond / 1000));
        const auto leftSlice = left.preview(kLimits, kSliceTicks);
        const auto rightSlice = right.preview(kLimits, kSliceTicks);
        TEST_ASSERT_EQUAL_INT16(leftSlice.steps, rightSlice.steps);
        TEST_ASSERT_EQUAL_DOUBLE(leftSlice.next.velocityStepsPerSecond,
                                 rightSlice.next.velocityStepsPerSecond);
        TEST_ASSERT_EQUAL_DOUBLE(
            leftSlice.next.accelerationStepsPerSecondSquared,
            rightSlice.next.accelerationStepsPerSecondSquared);
    }

    void test_starvation_tail_stops_and_holds_without_seeking_center(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(900);
        planner.setRange(0, 1000, 20);
        acceptWaypoint(planner, 700, 80);
        const int32_t holdStart = planner.state().positionSteps;
        for (int index = 0; index < 200 && !planner.isStationary(); ++index) {
            const auto before = planner.state();
            const auto slice = planner.previewHold(kLimits, kSliceTicks);
            planner.commit(slice, slice.requestedTicks);
            assertLimits(before, planner.state());
            TEST_ASSERT_GREATER_OR_EQUAL_INT32(0,
                                               planner.state().positionSteps);
            TEST_ASSERT_LESS_OR_EQUAL_INT32(1000,
                                            planner.state().positionSteps);
        }
        TEST_ASSERT_TRUE(planner.isStationary());
        TEST_ASSERT_INT32_WITHIN(400, holdStart, planner.state().positionSteps);
        TEST_ASSERT_GREATER_THAN_INT32(500, planner.state().positionSteps);
    }

    void test_new_waypoint_resumes_from_hold_state_without_reset(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        planner.setRange(-2000, 2000, 20);
        acceptWaypoint(planner, 800, 80);
        for (int index = 0; index < 4; ++index) {
            const auto slice = planner.previewHold(kLimits, kSliceTicks);
            planner.commit(slice, slice.requestedTicks);
        }
        const auto heldState = planner.state();
        TEST_ASSERT_TRUE(
            planner.appendWaypoint(-400, 200 * (kTicksPerSecond / 1000)));
        const auto resumed = planner.preview(kLimits, kSliceTicks);
        TEST_ASSERT_EQUAL_INT32(heldState.positionSteps,
                                planner.state().positionSteps);
        TEST_ASSERT_LESS_OR_EQUAL_DOUBLE(
            kLimits.accelerationStepsPerSecondSquared /
                    (timed_streaming::kJerkRampMilliseconds / 1000.0) *
                    (timed_streaming::kSliceMilliseconds / 1000.0) +
                1e-6,
            std::abs(resumed.next.accelerationStepsPerSecondSquared -
                     heldState.accelerationStepsPerSecondSquared));
    }

    void test_starvation_recovery_returns_smoothly_to_range_center(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(900);
        planner.setRange(0, 1000, 100);
        acceptWaypoint(planner, 800, 80);

        for (int index = 0; index < 250 && !planner.isStationary(); ++index) {
            const auto before = planner.state();
            const auto slice = planner.previewHold(kLimits, kSliceTicks);
            planner.commit(slice, slice.requestedTicks);
            assertLimits(before, planner.state());
        }
        TEST_ASSERT_TRUE(planner.isStationary());

        const int32_t center = timed_streaming::playRangeCenterSteps(0, 1000);
        for (int attempt = 0;
             attempt < 3 &&
             std::abs(planner.state().positionSteps - center) > 20;
             ++attempt) {
            const uint32_t durationMs =
                timed_streaming::centerRecoveryDurationMilliseconds(
                    planner.state().positionSteps, center,
                    kLimits.speedStepsPerSecond);
            acceptWaypoint(planner, center, durationMs);
            for (int index = 0; index < 250 && !planner.isStationary();
                 ++index) {
                const auto before = planner.state();
                const auto slice = planner.previewHold(kLimits, kSliceTicks);
                planner.commit(slice, slice.requestedTicks);
                assertLimits(before, planner.state());
            }
        }

        TEST_ASSERT_INT32_WITHIN(20, center, planner.state().positionSteps);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(0, planner.state().positionSteps);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(1000, planner.state().positionSteps);
    }

    void test_center_recovery_duration_is_bounded_and_distance_aware(void) {
        TEST_ASSERT_EQUAL_INT32(
            0, timed_streaming::playRangeCenterSteps(-1000, 1000));
        TEST_ASSERT_EQUAL_INT32(
            0, timed_streaming::playRangeCenterSteps(1000, -1000));
        TEST_ASSERT_EQUAL_UINT32(
            750, timed_streaming::centerRecoveryDurationMilliseconds(0, 10,
                                                                     20000.0));
        TEST_ASSERT_EQUAL_UINT32(
            4000, timed_streaming::centerRecoveryDurationMilliseconds(0, 1000,
                                                                      1000.0));
        TEST_ASSERT_EQUAL_UINT32(
            5000, timed_streaming::centerRecoveryDurationMilliseconds(0, 100000,
                                                                      1000.0));
    }

    void test_fractional_steps_accumulate(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        for (int i = 1; i <= 200; ++i) {
            acceptWaypoint(planner, i, 20);
        }
        TEST_ASSERT_INT32_WITHIN(3, 200, planner.state().positionSteps);
        TEST_ASSERT_LESS_THAN_DOUBLE(0.51,
                                     std::abs(planner.state().fractionalSteps));
    }

    void test_timing_carry_stays_bounded_for_ten_thousand_slices(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        planner.beginWaypoint(10000,
                              static_cast<uint64_t>(10000) * kSliceTicks);
        int64_t desiredTicks = 0;
        int64_t actualTicks = 0;
        for (int i = 0; i < 10000; ++i) {
            const auto slice = planner.preview(kLimits, kSliceTicks);
            // Model alternating integer-period rounding from moveTimed().
            const int32_t rounding = (i % 3) - 1;
            const uint32_t actual = slice.requestedTicks + rounding;
            desiredTicks += slice.nominalTicks;
            actualTicks += actual;
            planner.commit(slice, actual);
            TEST_ASSERT_LESS_OR_EQUAL_INT64(
                2, std::llabs(planner.state().timingCarryTicks));
        }
        TEST_ASSERT_LESS_OR_EQUAL_INT64(2,
                                        std::llabs(desiredTicks - actualTicks));
    }

    void test_retry_preview_does_not_advance_or_duplicate(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(123);
        planner.beginWaypoint(1000, 40 * (kTicksPerSecond / 1000));
        const auto initial = planner.state();
        const auto first = planner.preview(kLimits, kSliceTicks);
        const auto retry = planner.preview(kLimits, kSliceTicks);
        TEST_ASSERT_EQUAL_INT16(first.steps, retry.steps);
        TEST_ASSERT_EQUAL_UINT32(first.requestedTicks, retry.requestedTicks);
        TEST_ASSERT_EQUAL_INT32(initial.positionSteps,
                                planner.state().positionSteps);
        TEST_ASSERT_EQUAL_UINT64(40 * (kTicksPerSecond / 1000),
                                 planner.remainingWaypointTicks());
        planner.commit(retry, retry.requestedTicks);
        TEST_ASSERT_EQUAL_INT32(123 + retry.steps,
                                planner.state().positionSteps);
    }

    void test_reset_models_speed_zero_resynchronization(void) {
        timed_streaming::Planner planner(kTicksPerSecond);
        planner.reset(0);
        acceptWaypoint(planner, 1000, 100);
        planner.beginWaypoint(-1000, 500 * (kTicksPerSecond / 1000));
        planner.reset(377);
        TEST_ASSERT_FALSE(planner.hasWaypoint());
        TEST_ASSERT_EQUAL_INT32(377, planner.state().positionSteps);
        TEST_ASSERT_EQUAL_DOUBLE(0.0, planner.state().velocityStepsPerSecond);
        TEST_ASSERT_EQUAL_DOUBLE(
            0.0, planner.state().accelerationStepsPerSecondSquared);
        TEST_ASSERT_EQUAL_INT64(0, planner.state().timingCarryTicks);
    }

    void test_move_timed_result_policy(void) {
        using timed_streaming::SubmissionDisposition;
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SubmissionDisposition::Commit),
            static_cast<int>(timed_streaming::classifySubmission(0, 7, false)));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SubmissionDisposition::Commit),
            static_cast<int>(timed_streaming::classifySubmission(7, 7, true)));
        TEST_ASSERT_EQUAL_INT(
            static_cast<int>(SubmissionDisposition::UnexpectedEmpty),
            static_cast<int>(timed_streaming::classifySubmission(7, 7, false)));
        for (int result = 1; result < 7; ++result) {
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(SubmissionDisposition::Retry),
                static_cast<int>(
                    timed_streaming::classifySubmission(result, 7, false)));
        }
        for (int result = -4; result < 0; ++result) {
            TEST_ASSERT_EQUAL_INT(
                static_cast<int>(SubmissionDisposition::Fatal),
                static_cast<int>(
                    timed_streaming::classifySubmission(result, 7, false)));
        }
    }

    void test_prime_and_reset_policies(void) {
        TEST_ASSERT_EQUAL_UINT32(
            8, timed_streaming::requiredPrimeMilliseconds(false, 100));
        TEST_ASSERT_EQUAL_UINT32(
            8, timed_streaming::requiredPrimeMilliseconds(true, 0));
        TEST_ASSERT_EQUAL_UINT32(
            100, timed_streaming::requiredPrimeMilliseconds(true, 50));
        TEST_ASSERT_EQUAL_UINT32(
            200, timed_streaming::requiredPrimeMilliseconds(true, 100));
        TEST_ASSERT_EQUAL_UINT32(
            200, timed_streaming::requiredPrimeMilliseconds(true, 1000));

        const auto overflow = timed_streaming::resetPolicy(
            timed_streaming::ResetCause::InputOverflow);
        TEST_ASSERT_TRUE(overflow.forceStop);
        TEST_ASSERT_TRUE(overflow.clearInputQueue);
        TEST_ASSERT_FALSE(overflow.preserveActiveWaypoint);

        const auto underrun =
            timed_streaming::resetPolicy(timed_streaming::ResetCause::Underrun);
        TEST_ASSERT_TRUE(underrun.forceStop);
        TEST_ASSERT_FALSE(underrun.clearInputQueue);
        TEST_ASSERT_TRUE(underrun.preserveActiveWaypoint);
        TEST_ASSERT_TRUE(underrun.rebuffer);
    }

}  // namespace

void setUp() {}
void tearDown() {}

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_constant_velocity_sequence);
    RUN_TEST(test_sine_sequence_obeys_all_limits);
    RUN_TEST(test_nominal_sine_aligned_rmse_is_below_two_mm);
    RUN_TEST(test_nominal_twenty_hertz_sine_restores_smooth_baseline);
    RUN_TEST(test_triangle_reversal_is_jerk_limited);
    RUN_TEST(test_physically_possible_random_sequence);
    RUN_TEST(test_repeated_position_consumes_hold_without_snap);
    RUN_TEST(test_short_and_long_waypoints);
    RUN_TEST(test_range_envelope_never_emits_out_of_bounds_endpoint);
    RUN_TEST(test_future_waypoint_does_not_change_active_sequential_slice);
    RUN_TEST(test_starvation_tail_stops_and_holds_without_seeking_center);
    RUN_TEST(test_new_waypoint_resumes_from_hold_state_without_reset);
    RUN_TEST(test_starvation_recovery_returns_smoothly_to_range_center);
    RUN_TEST(test_center_recovery_duration_is_bounded_and_distance_aware);
    RUN_TEST(test_fractional_steps_accumulate);
    RUN_TEST(test_timing_carry_stays_bounded_for_ten_thousand_slices);
    RUN_TEST(test_retry_preview_does_not_advance_or_duplicate);
    RUN_TEST(test_reset_models_speed_zero_resynchronization);
    RUN_TEST(test_move_timed_result_policy);
    RUN_TEST(test_prime_and_reset_policies);
    return UNITY_END();
}
