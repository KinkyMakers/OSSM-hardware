#include "streaming.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include "constants/Config.h"
#include "ossm/state/calibration.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/communication/queue.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "streaming_logic.h"
#include "timed_streaming_planner.h"
#include "timed_streaming_runtime.h"

namespace sml = boost::sml;
using namespace sml;

namespace streaming {
    namespace {

        constexpr uint32_t kTicksPerMillisecond = TICKS_PER_S / 1000;
        constexpr uint32_t kSliceTicks =
            timed_streaming::kSliceMilliseconds * kTicksPerMillisecond;
        constexpr uint32_t kMaximumStepperQueueMilliseconds = 80;
        // ticksInQueue() excludes the command currently executing. Keeping
        // two slices of margin prevents a final append plus that active slice
        // from exceeding the 80 ms cap.
        constexpr uint32_t kStepperPendingTargetMilliseconds =
            kMaximumStepperQueueMilliseconds -
            2 * timed_streaming::kSliceMilliseconds;
        constexpr uint32_t kMaximumPrimeMilliseconds = 200;
        constexpr uint32_t kMaximumPlannerBufferMilliseconds = 200;
        constexpr uint32_t kMinimumPrimeMilliseconds =
            2 * timed_streaming::kSliceMilliseconds;

        std::atomic<bool> streamingTaskActive{false};
        std::atomic<bool> immediateStopRequested{false};
        std::atomic<uint32_t> immediateStopMicros{0};

        struct Diagnostics {
            uint32_t submittedSlices = 0;
            uint32_t retries = 0;
            uint32_t underruns = 0;
            uint32_t rebuffers = 0;
            uint32_t overflows = 0;
            uint32_t fatalErrors = 0;
            uint32_t zeroDurationWaypoints = 0;
            uint32_t maximumTimingCarryTicks = 0;
            uint32_t lastStopLatencyMicros = 0;
            uint32_t boundaryEnvelopeActivations = 0;
            uint32_t safetyClampActivations = 0;
            uint32_t reconciliationErrors = 0;
            int32_t maximumReconciliationErrorSteps = 0;
            int32_t maximumReferenceErrorSteps = 0;
            int32_t minimumLegalRangeMarginSteps = INT32_MAX;
            uint32_t starvationEvents = 0;
            uint32_t holdSlices = 0;
            uint32_t centerRecoveryEvents = 0;
            uint32_t centerRecoveryCompletions = 0;
        };

        uint32_t requiredPrimeMilliseconds() {
            return timed_streaming::requiredPrimeMilliseconds(
                USE_LATENCY_COMPENSATION, settings.buffer,
                kMinimumPrimeMilliseconds, kMaximumPrimeMilliseconds);
        }

        uint32_t waypointAgeMilliseconds(const PositionTime &waypoint) {
            const auto age =
                std::chrono::steady_clock::now() - waypoint.setTime;
            const auto millis =
                std::chrono::duration_cast<std::chrono::milliseconds>(age)
                    .count();
            return millis > 0 ? static_cast<uint32_t>(millis) : 0;
        }

        timed_streaming::Range activeLegalRange() {
            const int32_t maximumStroke = streaming_logic::calculateMaxStroke(
                settings.stroke, settings.depth,
                calibration.measuredStrokeSteps);
            const int32_t depthOffset = streaming_logic::calculateDepthOffset(
                calibration.measuredStrokeSteps, maximumStroke, settings.depth);
            const int32_t first = streaming_logic::scaleStreamPosition(
                0, maximumStroke, depthOffset);
            const int32_t last = streaming_logic::scaleStreamPosition(
                100, maximumStroke, depthOffset);
            const int32_t railMinimum = -static_cast<int32_t>(
                std::lround(std::abs(calibration.measuredStrokeSteps)));
            const int32_t railMaximum = 0;
            const int32_t minimum =
                std::max(railMinimum, std::min(first, last));
            const int32_t maximum =
                std::min(railMaximum, std::max(first, last));
            const int32_t span = std::max(0, maximum - minimum);
            const int32_t guard =
                std::max(static_cast<int32_t>(Config::Driver::stepsPerMM),
                         static_cast<int32_t>(std::ceil(span * 0.10)));
            return {minimum, maximum, std::min(guard, span / 2)};
        }

        int32_t mapTargetPosition(uint8_t requestedPosition) {
            const uint8_t clampedPosition =
                std::min<uint8_t>(100, requestedPosition);
            const int32_t maximumStroke = streaming_logic::calculateMaxStroke(
                settings.stroke, settings.depth,
                calibration.measuredStrokeSteps);
            const int32_t depthOffset = streaming_logic::calculateDepthOffset(
                calibration.measuredStrokeSteps, maximumStroke, settings.depth);
            const auto range = activeLegalRange();
            return std::max(
                range.minimumSteps,
                std::min(range.maximumSteps,
                         streaming_logic::scaleStreamPosition(
                             clampedPosition, maximumStroke, depthOffset)));
        }

        void logSummary(const Diagnostics &diagnostics) {
            ESP_LOGI(
                "Streaming",
                "STREAM_DIAG event=summary slices=%u retries=%u underruns=%u "
                "rebuffers=%u overflows=%u fatal=%u zero_duration=%u "
                "max_timing_carry_ticks=%u stop_latency_us=%u "
                "boundary_envelope=%u safety_clamp=%u reconcile_errors=%u "
                "max_reconcile_steps=%d max_reference_error_steps=%d "
                "min_legal_margin_steps=%d starvation_events=%u hold_slices=%u "
                "center_recoveries=%u center_recovery_completions=%u",
                diagnostics.submittedSlices, diagnostics.retries,
                diagnostics.underruns, diagnostics.rebuffers,
                diagnostics.overflows, diagnostics.fatalErrors,
                diagnostics.zeroDurationWaypoints,
                diagnostics.maximumTimingCarryTicks,
                diagnostics.lastStopLatencyMicros,
                diagnostics.boundaryEnvelopeActivations,
                diagnostics.safetyClampActivations,
                diagnostics.reconciliationErrors,
                diagnostics.maximumReconciliationErrorSteps,
                diagnostics.maximumReferenceErrorSteps,
                diagnostics.minimumLegalRangeMarginSteps == INT32_MAX
                    ? 0
                    : diagnostics.minimumLegalRangeMarginSteps,
                diagnostics.starvationEvents, diagnostics.holdSlices,
                diagnostics.centerRecoveryEvents,
                diagnostics.centerRecoveryCompletions);
        }

        void forceSpeedZero() {
            settings.speed = 0;
            settings.speedBLE = 0;
        }

        void stopTimedQueueImmediately() {
            if (stepper == nullptr) return;
            // forceStop() is the interrupt-safe stop signal required by
            // Streaming. A timed queue can contain up to 80 ms, so immediately
            // flush the ignored remainder while keeping the enable output
            // asserted and preserving the best available physical position
            // estimate.
            stepper->forceStop();
            const int32_t stoppedPosition = stepper->getCurrentPosition();
            stepper->forceStopAndNewPosition(stoppedPosition);
        }

        void fatalStop(const char *reason, int result,
                       Diagnostics &diagnostics) {
            ++diagnostics.fatalErrors;
            forceSpeedZero();
            clearTargetQueue();
            stopTimedQueueImmediately();
            ESP_LOGE(
                "Streaming",
                "STREAM_ERROR type=fatal reason=%s result=%d fatal_count=%u",
                reason, result, diagnostics.fatalErrors);
        }

        void preserveWaypointAcrossResync(timed_streaming::Planner &planner) {
            planner.stopAndResynchronize(stepper->getCurrentPosition());
        }

        bool fillSequentialWaypoints(timed_streaming::Planner &planner,
                                     PositionTime &oldestWaypoint,
                                     bool &hasOldestWaypoint,
                                     bool &acceptedInput,
                                     Diagnostics &diagnostics) {
            acceptedInput = false;
            while (
                planner.canBufferWaypoint() &&
                planner.bufferedTicks() <
                    static_cast<uint64_t>(kMaximumPlannerBufferMilliseconds) *
                        kTicksPerMillisecond) {
                PositionTime candidate{};
                if (!dequeueTarget(candidate)) break;
                if (candidate.inTime == 0) {
                    ++diagnostics.zeroDurationWaypoints;
                    ESP_LOGW(
                        "Streaming",
                        "STREAM_DIAG event=ignored_zero_duration position=%u "
                        "count=%u",
                        candidate.position, diagnostics.zeroDurationWaypoints);
                    continue;
                }
                if (!hasOldestWaypoint) {
                    oldestWaypoint = candidate;
                    hasOldestWaypoint = true;
                }
                if (!planner.appendWaypoint(
                        mapTargetPosition(candidate.position),
                        static_cast<uint64_t>(candidate.inTime) *
                            kTicksPerMillisecond)) {
                    fatalStop("planner_overflow", 0, diagnostics);
                    return false;
                }
                acceptedInput = true;
            }
            return planner.hasWaypoint();
        }

        uint32_t bufferedMilliseconds(const timed_streaming::Planner &planner) {
            const uint64_t activeMilliseconds =
                (planner.bufferedTicks() + kTicksPerMillisecond - 1) /
                kTicksPerMillisecond;
            const uint64_t stepperMilliseconds =
                stepper == nullptr
                    ? 0
                    : (stepper->ticksInQueue() + kTicksPerMillisecond - 1) /
                          kTicksPerMillisecond;
            const uint64_t total = activeMilliseconds +
                                   targetQueueBufferedDurationMs() +
                                   stepperMilliseconds;
            return static_cast<uint32_t>(std::min<uint64_t>(total, UINT32_MAX));
        }

        enum class SubmitResult {
            Accepted,
            Retry,
            UnexpectedEmpty,
            StopRequested,
            Fatal,
            NoData
        };

        SubmitResult submitOneSlice(timed_streaming::Planner &planner,
                                    PositionTime &activeWaypoint,
                                    bool &hasActiveWaypoint,
                                    const timed_streaming::Limits &limits,
                                    bool start, bool priming, bool hold,
                                    Diagnostics &diagnostics) {
            if (immediateStopRequested.load(std::memory_order_acquire)) {
                return SubmitResult::StopRequested;
            }
            if (!planner.hasWaypoint() && !hold) {
                return SubmitResult::NoData;
            }

            const auto slice = hold ? planner.previewHold(limits, kSliceTicks)
                                    : planner.preview(limits, kSliceTicks);
            if (slice.nominalTicks == 0 || slice.requestedTicks == 0) {
                fatalStop("invalid_slice", 0, diagnostics);
                return SubmitResult::Fatal;
            }

            uint32_t actualDuration = 0;
            const MoveTimedResultCode moveResult = stepper->moveTimed(
                slice.steps, slice.requestedTicks, &actualDuration, start);
            const int result = static_cast<int>(moveResult);

            // requestImmediateStop() can race this call from the BLE task.
            // Stop again after moveTimed() so a slice appended after the
            // caller's forceStop() cannot survive or be committed.
            if (immediateStopRequested.load(std::memory_order_acquire)) {
                stopTimedQueueImmediately();
                return SubmitResult::StopRequested;
            }

            const auto disposition = timed_streaming::classifySubmission(
                result, static_cast<int>(MOVE_TIMED_EMPTY), priming);
            if (disposition == timed_streaming::SubmissionDisposition::Commit) {
                planner.commit(slice, actualDuration);
                ++diagnostics.submittedSlices;
                if (slice.boundaryEnvelopeActive)
                    ++diagnostics.boundaryEnvelopeActivations;
                if (slice.safetyClampActive) {
                    ++diagnostics.safetyClampActivations;
                    ESP_LOGW(
                        "Streaming",
                        "STREAM_DIAG event=safety_clamp planned_position=%d "
                        "margin_steps=%d count=%u",
                        planner.state().positionSteps,
                        static_cast<int32_t>(
                            std::lround(slice.minimumRangeMarginSteps)),
                        diagnostics.safetyClampActivations);
                }
                if (hold) ++diagnostics.holdSlices;
                diagnostics.maximumReferenceErrorSteps = std::max(
                    diagnostics.maximumReferenceErrorSteps,
                    static_cast<int32_t>(
                        std::lround(std::abs(slice.referenceErrorSteps))));
                diagnostics.minimumLegalRangeMarginSteps =
                    std::min(diagnostics.minimumLegalRangeMarginSteps,
                             static_cast<int32_t>(
                                 std::floor(slice.minimumRangeMarginSteps)));
                diagnostics.maximumTimingCarryTicks =
                    std::max(diagnostics.maximumTimingCarryTicks,
                             static_cast<uint32_t>(
                                 std::llabs(planner.state().timingCarryTicks)));
                if (!planner.hasWaypoint()) hasActiveWaypoint = false;
                const int32_t queuedPosition =
                    stepper->getPositionAfterCommandsCompleted();
                const int32_t reconciliationError =
                    queuedPosition - planner.state().positionSteps;
                diagnostics.maximumReconciliationErrorSteps = std::max(
                    diagnostics.maximumReconciliationErrorSteps,
                    static_cast<int32_t>(std::abs(reconciliationError)));
                if (std::abs(reconciliationError) > 1) {
                    ++diagnostics.reconciliationErrors;
                    ESP_LOGE("Streaming",
                             "STREAM_ERROR type=reconciliation error_steps=%d "
                             "queued_position=%d planned_position=%d count=%u",
                             reconciliationError, queuedPosition,
                             planner.state().positionSteps,
                             diagnostics.reconciliationErrors);
                }
                planner.reconcilePosition(queuedPosition);
                ESP_LOGV(
                    "Streaming",
                    "STREAM_SLICE n=%u steps=%d requested_ticks=%u "
                    "actual_ticks=%u carry_ticks=%lld queue_ms=%u "
                    "current_position=%d future_position=%d reconcile_steps=%d "
                    "reference_error_steps=%.3f range_margin_steps=%.3f "
                    "boundary_envelope=%d safety_clamp=%d starvation_hold=%d",
                    diagnostics.submittedSlices, slice.steps,
                    slice.requestedTicks, actualDuration,
                    static_cast<long long>(planner.state().timingCarryTicks),
                    stepper->ticksInQueue() / kTicksPerMillisecond,
                    stepper->getCurrentPosition(), queuedPosition,
                    reconciliationError, slice.referenceErrorSteps,
                    slice.minimumRangeMarginSteps,
                    slice.boundaryEnvelopeActive ? 1 : 0,
                    slice.safetyClampActive ? 1 : 0, hold ? 1 : 0);
                return SubmitResult::Accepted;
            }

            if (disposition ==
                timed_streaming::SubmissionDisposition::UnexpectedEmpty) {
                ++diagnostics.underruns;
                stopTimedQueueImmediately();
                ESP_LOGW("Streaming",
                         "STREAM_DIAG event=underrun source=moveTimed count=%u",
                         diagnostics.underruns);
                return SubmitResult::UnexpectedEmpty;
            }

            if (disposition == timed_streaming::SubmissionDisposition::Retry) {
                ++diagnostics.retries;
                ESP_LOGD("Streaming",
                         "STREAM_DIAG event=retry result=%d count=%u", result,
                         diagnostics.retries);
                return SubmitResult::Retry;
            }

            fatalStop("move_timed", result, diagnostics);
            return SubmitResult::Fatal;
        }

        SubmitResult kickPrimedQueue(Diagnostics &diagnostics) {
            if (immediateStopRequested.load(std::memory_order_acquire)) {
                return SubmitResult::StopRequested;
            }
            const MoveTimedResultCode kickResult =
                stepper->moveTimed(0, 0, nullptr, true);
            if (immediateStopRequested.load(std::memory_order_acquire)) {
                stopTimedQueueImmediately();
                return SubmitResult::StopRequested;
            }
            const int result = static_cast<int>(kickResult);
            if (kickResult == MOVE_TIMED_OK) return SubmitResult::Accepted;
            if (result > 0) {
                ++diagnostics.retries;
                ESP_LOGD("Streaming",
                         "STREAM_DIAG event=kick_retry result=%d count=%u",
                         result, diagnostics.retries);
                return SubmitResult::Retry;
            }
            fatalStop("prime_kick", result, diagnostics);
            return SubmitResult::Fatal;
        }

        void startStreamingTask(void *) {
            stepperTranslateFrame(StepperFrame::Native);
            stepper->setDirectionPin(Pins::Driver::motorDirectionPin, false);
            stepper->enableOutputs();
            streamingTaskActive.store(true, std::memory_order_release);

            auto isInCorrectState = []() {
                return stateMachine->is("streaming"_s) ||
                       stateMachine->is("streaming.preflight"_s) ||
                       stateMachine->is("streaming.idle"_s);
            };

            timed_streaming::Planner planner(TICKS_PER_S);
            planner.reset(stepper->getCurrentPosition());
            auto legalRange = activeLegalRange();
            planner.setRange(legalRange.minimumSteps, legalRange.maximumSteps,
                             legalRange.guardSteps);
            PositionTime activeWaypoint{};
            bool hasActiveWaypoint = false;
            bool hasEverStreamed = false;
            bool drainingToHold = false;
            bool starvationActive = false;
            bool recoveringToCenter = false;
            bool queueStarted = false;
            bool waitingForStop = false;
            bool preserveWaypointOnStop = false;
            bool rebuffering = false;
            uint32_t observedOverflowCount = targetQueueOverflowCount();
            Diagnostics diagnostics{};

            while (isInCorrectState()) {
                if (targetQueueOverflowCount() != observedOverflowCount) {
                    observedOverflowCount = targetQueueOverflowCount();
                    ++diagnostics.overflows;
                    fatalStop("input_overflow", 0, diagnostics);
                    queueStarted = false;
                    waitingForStop = true;
                    preserveWaypointOnStop = false;
                    hasActiveWaypoint = false;
                }

                if (immediateStopRequested.exchange(
                        false, std::memory_order_acq_rel)) {
                    waitingForStop = true;
                    preserveWaypointOnStop = false;
                    queueStarted = false;
                    hasActiveWaypoint = false;
                    drainingToHold = false;
                    starvationActive = false;
                    recoveringToCenter = false;
                }

                if (waitingForStop) {
                    if (stepper->isRunning()) {
                        vTaskDelay(1);
                        continue;
                    }
                    if (preserveWaypointOnStop) {
                        preserveWaypointAcrossResync(planner);
                    } else {
                        planner.reset(stepper->getCurrentPosition());
                    }
                    preserveWaypointOnStop = false;
                    const uint32_t requestedAt =
                        immediateStopMicros.load(std::memory_order_acquire);
                    if (requestedAt != 0) {
                        diagnostics.lastStopLatencyMicros =
                            micros() - requestedAt;
                        immediateStopMicros.store(0, std::memory_order_release);
                        ESP_LOGI("Streaming",
                                 "STREAM_DIAG event=stop latency_us=%u",
                                 diagnostics.lastStopLatencyMicros);
                    }
                    waitingForStop = false;
                }

                const uint32_t speedLimit = static_cast<uint32_t>(
                    Config::Driver::maxSpeedMmPerSecond *
                    Config::Driver::stepsPerMM *
                    std::max(0.0f, settings.speed) / 100.0f);
                const uint32_t accelerationLimit = static_cast<uint32_t>(
                    Config::Driver::maxAcceleration *
                    Config::Driver::stepsPerMM *
                    std::max(0.0f, settings.sensation) / 100.0f);
                if (speedLimit == 0 || accelerationLimit == 0) {
                    if (queueStarted || stepper->isRunning()) {
                        clearTargetQueue();
                        stopTimedQueueImmediately();
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                        queueStarted = false;
                        hasActiveWaypoint = false;
                    }
                    vTaskDelay(1);
                    continue;
                }
                const timed_streaming::Limits limits{
                    static_cast<double>(speedLimit),
                    static_cast<double>(accelerationLimit)};

                const auto updatedRange = activeLegalRange();
                if (updatedRange.minimumSteps != legalRange.minimumSteps ||
                    updatedRange.maximumSteps != legalRange.maximumSteps ||
                    updatedRange.guardSteps != legalRange.guardSteps) {
                    legalRange = updatedRange;
                    planner.setRange(legalRange.minimumSteps,
                                     legalRange.maximumSteps,
                                     legalRange.guardSteps);
                    ESP_LOGI(
                        "Streaming",
                        "STREAM_DIAG event=range_change min=%d max=%d guard=%d",
                        legalRange.minimumSteps, legalRange.maximumSteps,
                        legalRange.guardSteps);
                    if (queueStarted || stepper->isRunning()) {
                        stopTimedQueueImmediately();
                        queueStarted = false;
                        waitingForStop = true;
                        preserveWaypointOnStop = true;
                        rebuffering = true;
                        continue;
                    }
                }

                // A fully starved queue is already stopped. Reconcile once
                // before accepting new data; never clear or force-stop merely
                // because data resumed.
                if (starvationActive && targetQueueSize() != 0 &&
                    !stepper->isRunning()) {
                    planner.stopAndResynchronize(stepper->getCurrentPosition());
                }

                bool acceptedInput = false;
                const bool hasWaypoints = fillSequentialWaypoints(
                    planner, activeWaypoint, hasActiveWaypoint, acceptedInput,
                    diagnostics);
                if (acceptedInput) {
                    hasEverStreamed = true;
                    if (starvationActive || drainingToHold) {
                        ESP_LOGI(
                            "Streaming",
                            "STREAM_DIAG event=data_resumed buffered_ms=%u",
                            bufferedMilliseconds(planner));
                    }
                    starvationActive = false;
                    drainingToHold = false;
                    recoveringToCenter = false;
                } else if (hasEverStreamed && targetQueueSize() == 0 &&
                           !hasWaypoints && !planner.isStationary()) {
                    // No reference remains. Append a single smooth braking
                    // tail behind already queued motion, then stop generating.
                    drainingToHold = true;
                }

                bool useHold = drainingToHold && !planner.hasWaypoint();
                bool hasWork = planner.hasWaypoint() || useHold;
                if (!hasWork) {
                    queueStarted = stepper->isQueueRunning();
                    if (hasEverStreamed && targetQueueSize() == 0 &&
                        !queueStarted && stepper->isQueueEmpty() &&
                        planner.isStationary()) {
                        planner.stopAndResynchronize(
                            stepper->getCurrentPosition());
                        if (!starvationActive) {
                            starvationActive = true;
                            ++diagnostics.starvationEvents;
                            ESP_LOGI(
                                "Streaming",
                                "STREAM_DIAG event=starved hold_position=%d "
                                "count=%u",
                                planner.state().positionSteps,
                                diagnostics.starvationEvents);
                        }

                        const int32_t center =
                            timed_streaming::playRangeCenterSteps(
                                legalRange.minimumSteps,
                                legalRange.maximumSteps);
                        const int32_t centerTolerance =
                            std::max<int32_t>(1, Config::Driver::stepsPerMM);
                        if (recoveringToCenter) {
                            recoveringToCenter = false;
                            ++diagnostics.centerRecoveryCompletions;
                            ESP_LOGI(
                                "Streaming",
                                "STREAM_DIAG event=center_recovery_complete "
                                "position=%d center=%d count=%u",
                                planner.state().positionSteps, center,
                                diagnostics.centerRecoveryCompletions);
                        }
                        if (std::abs(planner.state().positionSteps - center) >
                            centerTolerance) {
                            const uint32_t recoveryMs = timed_streaming::
                                centerRecoveryDurationMilliseconds(
                                    planner.state().positionSteps, center,
                                    speedLimit);
                            if (planner.appendWaypoint(
                                    center, static_cast<uint64_t>(recoveryMs) *
                                                kTicksPerMillisecond)) {
                                recoveringToCenter = true;
                                ++diagnostics.centerRecoveryEvents;
                                ESP_LOGI(
                                    "Streaming",
                                    "STREAM_DIAG event=center_recovery_start "
                                    "position=%d center=%d duration_ms=%u "
                                    "count=%u",
                                    planner.state().positionSteps, center,
                                    recoveryMs,
                                    diagnostics.centerRecoveryEvents);
                            }
                        }
                    }
                    vTaskDelay(1);
                    continue;
                }

                if (!queueStarted || !stepper->isQueueRunning()) {
                    if (queueStarted) {
                        ++diagnostics.underruns;
                        rebuffering = true;
                        planner.stopAndResynchronize(
                            stepper->getCurrentPosition());
                        ESP_LOGW(
                            "Streaming",
                            "STREAM_DIAG event=underrun source=empty_queue "
                            "count=%u",
                            diagnostics.underruns);
                    }
                    queueStarted = false;

                    const uint32_t primeMs = requiredPrimeMilliseconds();
                    if (!useHold && bufferedMilliseconds(planner) < primeMs &&
                        waypointAgeMilliseconds(activeWaypoint) < primeMs) {
                        vTaskDelay(1);
                        continue;
                    }

                    if (rebuffering) {
                        ++diagnostics.rebuffers;
                        ESP_LOGI("Streaming",
                                 "STREAM_DIAG event=rebuffer count=%u "
                                 "buffered_ms=%u",
                                 diagnostics.rebuffers,
                                 bufferedMilliseconds(planner));
                    }

                    bool primingFatal = false;
                    bool primingStopped = false;
                    while (stepper->ticksInQueue() <
                           kStepperPendingTargetMilliseconds *
                               kTicksPerMillisecond) {
                        if (useHold && planner.isStationary()) break;
                        const SubmitResult result = submitOneSlice(
                            planner, activeWaypoint, hasActiveWaypoint, limits,
                            false, true, useHold, diagnostics);
                        if (result == SubmitResult::Accepted) continue;
                        if (result == SubmitResult::Retry) vTaskDelay(1);
                        primingStopped = result == SubmitResult::StopRequested;
                        primingFatal = result == SubmitResult::Fatal;
                        break;
                    }

                    if (primingStopped) {
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                        hasActiveWaypoint = false;
                    } else if (primingFatal) {
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                        hasActiveWaypoint = false;
                    } else if (!stepper->isQueueEmpty()) {
                        const SubmitResult kickResult =
                            kickPrimedQueue(diagnostics);
                        if (kickResult == SubmitResult::Accepted) {
                            queueStarted = true;
                            rebuffering = false;
                            ESP_LOGI(
                                "Streaming",
                                "STREAM_DIAG event=primed queue_ms=%u "
                                "input_ms=%u",
                                stepper->ticksInQueue() / kTicksPerMillisecond,
                                bufferedMilliseconds(planner));
                        } else if (kickResult == SubmitResult::StopRequested) {
                            waitingForStop = true;
                            preserveWaypointOnStop = false;
                            hasActiveWaypoint = false;
                        } else if (kickResult == SubmitResult::Fatal) {
                            waitingForStop = true;
                            preserveWaypointOnStop = false;
                            hasActiveWaypoint = false;
                        }
                    }
                    vTaskDelay(1);
                    continue;
                }

                while (stepper->ticksInQueue() <
                       kStepperPendingTargetMilliseconds *
                           kTicksPerMillisecond) {
                    useHold = drainingToHold && !planner.hasWaypoint();
                    if (!planner.hasWaypoint() && !useHold) break;
                    if (useHold && planner.isStationary()) {
                        drainingToHold = false;
                        break;
                    }
                    const SubmitResult result = submitOneSlice(
                        planner, activeWaypoint, hasActiveWaypoint, limits,
                        true, false, useHold, diagnostics);
                    if (result == SubmitResult::Accepted) continue;
                    if (result == SubmitResult::StopRequested) {
                        queueStarted = false;
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                        hasActiveWaypoint = false;
                    } else if (result == SubmitResult::UnexpectedEmpty) {
                        queueStarted = false;
                        waitingForStop = true;
                        preserveWaypointOnStop = true;
                        rebuffering = true;
                    } else if (result == SubmitResult::Fatal) {
                        queueStarted = false;
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                    }
                    break;
                }

                vTaskDelay(1);
            }

            streamingTaskActive.store(false, std::memory_order_release);
            logSummary(diagnostics);
            vTaskDelete(nullptr);
        }

    }  // namespace

    void requestImmediateStop() {
        // Clearing here establishes ordering: waypoints received after the
        // speed zero command survive, while all previously pending work is
        // discarded.
        immediateStopMicros.store(micros(), std::memory_order_release);
        immediateStopRequested.store(true, std::memory_order_release);
        clearTargetQueue();
        if (streamingTaskActive.load(std::memory_order_acquire) &&
            stepper != nullptr) {
            stopTimedQueueImmediately();
        }
    }

    void startStreaming() {
        clearTargetQueue();
        immediateStopRequested.store(false, std::memory_order_release);
        immediateStopMicros.store(0, std::memory_order_release);
        constexpr int stackSize = 10 * configMINIMAL_STACK_SIZE;
        xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask",
                                stackSize, nullptr, configMAX_PRIORITIES - 1,
                                nullptr, Tasks::operationTaskCore);
    }

}  // namespace streaming
