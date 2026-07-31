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
#include "services/communication/priority.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "stream_backlog_policy.h"
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
        constexpr uint32_t kMaximumPrimeMilliseconds = 200;
        constexpr uint32_t kMaximumPlannerBufferMilliseconds = 200;
        constexpr uint32_t kMinimumPrimeMilliseconds =
            2 * timed_streaming::kSliceMilliseconds;
        constexpr uint32_t kMomentumWaypointMilliseconds = 20;
        constexpr uint32_t kFixedResumeBlendMilliseconds = 100;

        std::atomic<bool> streamingTaskActive{false};
        std::atomic<bool> immediateStopRequested{false};
        std::atomic<uint32_t> immediateStopMicros{0};

        portMUX_TYPE tuningMux = portMUX_INITIALIZER_UNLOCKED;
        timed_streaming::TuningParameters activeTuning{};
        uint32_t tuningRevision = 0;

        timed_streaming::TuningParameters currentTuningParameters() {
            portENTER_CRITICAL(&tuningMux);
            const auto snapshot = activeTuning;
            portEXIT_CRITICAL(&tuningMux);
            return snapshot;
        }

        uint32_t executionHorizonMilliseconds(
            const timed_streaming::TuningParameters &parameters) {
#ifdef OSSM_STREAM_TUNING
            return parameters.executionHorizonMilliseconds;
#else
            (void)parameters;
            // ticksInQueue() excludes the command currently executing. Keep
            // two slices of margin below the fixed 80 ms driver cap.
            return kMaximumStepperQueueMilliseconds -
                   2 * timed_streaming::kSliceMilliseconds;
#endif
        }

        struct MomentumRecovery {
            bool active = false;
            uint32_t elapsedMilliseconds = 0;
            double originSteps = 0.0;
            double initialVelocityStepsPerSecond = 0.0;
            double virtualPositionSteps = 0.0;
            double virtualVelocityStepsPerSecond = 0.0;

            void reset() { *this = {}; }

            void begin(double positionSteps,
                       double referenceVelocityStepsPerSecond) {
                active = true;
                elapsedMilliseconds = 0;
                originSteps = positionSteps;
                initialVelocityStepsPerSecond =
                    referenceVelocityStepsPerSecond;
                virtualPositionSteps = positionSteps;
                virtualVelocityStepsPerSecond =
                    referenceVelocityStepsPerSecond;
            }
        };

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
            uint32_t backlogCompactions = 0;
            uint32_t droppedWaypoints = 0;
            uint32_t maximumInputAgeMilliseconds = 0;
            uint32_t maximumDiscardedAgeMilliseconds = 0;
        };

        uint32_t requiredPrimeMilliseconds(
            const timed_streaming::TuningParameters &parameters) {
#ifdef OSSM_STREAM_TUNING
            return parameters.primeMilliseconds;
#else
            (void)parameters;
            return timed_streaming::requiredPrimeMilliseconds(
                USE_LATENCY_COMPENSATION, settings.buffer,
                kMinimumPrimeMilliseconds, kMaximumPrimeMilliseconds);
#endif
        }

        uint32_t waypointAgeMilliseconds(const PositionTime &waypoint) {
            const auto age =
                std::chrono::steady_clock::now() - waypoint.receivedAt;
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
                streaming_logic::clampStreamPosition(requestedPosition);
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
                "center_recoveries=%u center_recovery_completions=%u "
                "backlog_compactions=%u dropped_waypoints=%u "
                "max_input_age_ms=%u max_discarded_age_ms=%u",
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
                diagnostics.centerRecoveryCompletions,
                diagnostics.backlogCompactions,
                diagnostics.droppedWaypoints,
                diagnostics.maximumInputAgeMilliseconds,
                diagnostics.maximumDiscardedAgeMilliseconds);
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
#ifdef OSSM_STREAM_TUNING
            resetTuningParameters();
#endif
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
                                     bool &resumeBlendPending,
                                     timed_streaming::ReferenceVelocityEstimator
                                         &velocityEstimator,
                                     Diagnostics &diagnostics) {
            acceptedInput = false;
            while (
                planner.canBufferWaypoint() &&
                planner.bufferedTicks() <
                    static_cast<uint64_t>(kMaximumPlannerBufferMilliseconds) *
                        kTicksPerMillisecond) {
                PositionTime candidate{};
                TargetQueueRead queueRead{};
                if (!dequeueFreshTarget(
                        candidate,
                        stream_backlog_policy::
                            kMaximumWaypointAgeMilliseconds,
                        stream_backlog_policy::
                            kMaximumQueuedDurationMilliseconds,
                        queueRead))
                    break;
                diagnostics.maximumInputAgeMilliseconds = std::max(
                    diagnostics.maximumInputAgeMilliseconds,
                    queueRead.selectedAgeMilliseconds);
                if (queueRead.droppedWaypoints != 0) {
                    ++diagnostics.backlogCompactions;
                    diagnostics.droppedWaypoints +=
                        queueRead.droppedWaypoints;
                    diagnostics.maximumDiscardedAgeMilliseconds = std::max(
                        diagnostics.maximumDiscardedAgeMilliseconds,
                        queueRead.oldestAgeMilliseconds);
                    if (diagnostics.backlogCompactions <= 3 ||
                        diagnostics.backlogCompactions % 25 == 0) {
                        ESP_LOGW(
                            "Streaming",
                            "STREAM_DIAG event=backlog_compacted dropped=%u "
                            "oldest_age_ms=%u selected_age_ms=%u "
                            "buffered_before_ms=%u selected_sequence=%u "
                            "count=%u",
                            queueRead.droppedWaypoints,
                            queueRead.oldestAgeMilliseconds,
                            queueRead.selectedAgeMilliseconds,
                            queueRead.bufferedDurationBeforeMilliseconds,
                            queueRead.selectedSequence,
                            diagnostics.backlogCompactions);
                    }
                }
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
                const int32_t mappedPosition =
                    mapTargetPosition(candidate.position);
                velocityEstimator.record(mappedPosition, candidate.inTime);
                const uint32_t durationMilliseconds =
                    resumeBlendPending
                        ? std::max<uint32_t>(candidate.inTime,
                                             kFixedResumeBlendMilliseconds)
                        : candidate.inTime;
                resumeBlendPending = false;
                if (!planner.appendWaypoint(
                        mappedPosition,
                        static_cast<uint64_t>(durationMilliseconds) *
                            kTicksPerMillisecond)) {
                    fatalStop("planner_overflow", 0, diagnostics);
                    return false;
                }
                acceptedInput = true;
            }
            return planner.hasWaypoint();
        }

        bool appendMomentumWaypoint(
            timed_streaming::Planner &planner, MomentumRecovery &momentum,
            const timed_streaming::Range &range,
            const timed_streaming::TuningParameters &parameters,
            const timed_streaming::Limits &limits) {
            if (!momentum.active || !planner.canBufferWaypoint()) return false;

            const int32_t innerMinimum =
                range.minimumSteps + range.guardSteps;
            const int32_t innerMaximum =
                range.maximumSteps - range.guardSteps;
            const double center =
                timed_streaming::playRangeCenterSteps(innerMinimum,
                                                      innerMaximum);
            const double dt = kMomentumWaypointMilliseconds / 1000.0;
            momentum.elapsedMilliseconds += kMomentumWaypointMilliseconds;

            // Dropout motion is a damped mass in a center-seeking potential.
            // Its initial velocity carries the last four-point reference
            // estimate forward, while exponential drag, the center spring,
            // and the edge field continuously reshape the trajectory.
            const auto nextMass = timed_streaming::advanceDropoutMass(
                {momentum.virtualPositionSteps,
                 momentum.virtualVelocityStepsPerSecond},
                momentum.originSteps,
                momentum.initialVelocityStepsPerSecond,
                innerMinimum, innerMaximum, limits, parameters, dt);
            momentum.virtualPositionSteps = nextMass.positionSteps;
            momentum.virtualVelocityStepsPerSecond =
                nextMass.velocityStepsPerSecond;
            const double centerTolerance =
                std::max(1.0, static_cast<double>(Config::Driver::stepsPerMM));
            const double velocityTolerance = centerTolerance * 2.0;
            if (std::abs(momentum.virtualPositionSteps - center) <=
                    centerTolerance &&
                std::abs(momentum.virtualVelocityStepsPerSecond) <=
                    velocityTolerance) {
                momentum.virtualPositionSteps = center;
                momentum.virtualVelocityStepsPerSecond = 0.0;
                momentum.active = false;
            }
            return planner.appendWaypoint(
                static_cast<int32_t>(
                    std::lround(momentum.virtualPositionSteps)),
                static_cast<uint64_t>(kMomentumWaypointMilliseconds) *
                    kTicksPerMillisecond);
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

            auto tuning = currentTuningParameters();
            timed_streaming::Planner planner(
                TICKS_PER_S, tuning.jerkRampMilliseconds);
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
            bool resumeBlendPending = false;
            timed_streaming::ReferenceVelocityEstimator velocityEstimator;
            MomentumRecovery momentum;
            uint32_t observedOverflowCount = targetQueueOverflowCount();
            Diagnostics diagnostics{};

            while (isInCorrectState()) {
                if (targetQueueOverflowCount() != observedOverflowCount) {
                    const uint32_t currentOverflowCount =
                        targetQueueOverflowCount();
                    const uint32_t replacements =
                        currentOverflowCount - observedOverflowCount;
                    observedOverflowCount = currentOverflowCount;
                    diagnostics.overflows += replacements;
                    diagnostics.droppedWaypoints += replacements;
                    if (diagnostics.overflows <= 3 ||
                        diagnostics.overflows % 25 == 0) {
                        ESP_LOGW(
                            "Streaming",
                            "STREAM_DIAG event=input_overflow_recovered "
                            "replaced_oldest=%u total=%u queue_size=%u "
                            "buffered_ms=%u",
                            replacements, diagnostics.overflows,
                            static_cast<unsigned>(targetQueueSize()),
                            targetQueueBufferedDurationMs());
                    }
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
                    resumeBlendPending = false;
                    velocityEstimator.reset();
                    momentum.reset();
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

                tuning = currentTuningParameters();
                planner.setJerkRampMilliseconds(
                    tuning.jerkRampMilliseconds);
                planner.setMomentumDecayMilliseconds(
                    tuning.momentumDecayMilliseconds);
                planner.setEdgeRepulsionStrength(
                    tuning.edgeRepulsionStrength);

                const uint32_t speedLimit =
                    streaming_logic::calculateStreamingSpeedLimit(
                        settings.speed, Config::Driver::stepsPerMM);
                const uint32_t accelerationLimit =
                    streaming_logic::calculateStreamingAccelerationLimit(
                        settings.sensation,
#ifdef OSSM_STREAM_TUNING
                        tuning.accelerationScale,
#else
                        1.0f,
#endif
                        Config::Driver::stepsPerMM);
                if (speedLimit == 0 || accelerationLimit == 0) {
                    // A speed-zero command is an explicit stop boundary. BLE
                    // writes already in flight can still arrive afterward;
                    // discard them instead of letting the fixed input queue
                    // fill and turn a safe stop into an input-overflow fault.
                    // New waypoints are accepted normally as soon as both
                    // limits become nonzero again.
                    if (targetQueueSize() != 0) clearTargetQueue();
                    if (queueStarted || stepper->isRunning()) {
                        stopTimedQueueImmediately();
                        waitingForStop = true;
                        preserveWaypointOnStop = false;
                        queueStarted = false;
                        hasActiveWaypoint = false;
                    }
                    // Speed zero starts a new streaming epoch. Do not let a
                    // later nonzero speed resurrect starvation recovery from
                    // the preceding case before a fresh waypoint arrives.
                    hasEverStreamed = false;
                    drainingToHold = false;
                    starvationActive = false;
                    recoveringToCenter = false;
                    resumeBlendPending = false;
                    velocityEstimator.reset();
#ifdef OSSM_STREAM_TUNING
                    momentum.reset();
#endif
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
#ifdef OSSM_STREAM_TUNING
                if (momentum.active && targetQueueSize() != 0)
                    resumeBlendPending = true;
#endif
                bool hasWaypoints = fillSequentialWaypoints(
                    planner, activeWaypoint, hasActiveWaypoint, acceptedInput,
                    resumeBlendPending, velocityEstimator, diagnostics);
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
#ifdef OSSM_STREAM_TUNING
                    momentum.reset();
#endif
                } else if (hasEverStreamed && targetQueueSize() == 0 &&
                           !hasWaypoints && !planner.isStationary()) {
#ifdef OSSM_STREAM_TUNING
                    if (!momentum.active) {
                        momentum.begin(
                            planner.state().continuousPositionSteps,
                            velocityEstimator.velocityStepsPerSecond());
                        starvationActive = true;
                        ++diagnostics.starvationEvents;
                        ESP_LOGI(
                            "Streaming",
                            "STREAM_DIAG event=momentum_start position=%d "
                            "velocity_steps_s=%.3f decay_ms=%u coast_fraction=%.4f "
                            "count=%u",
                            planner.state().positionSteps,
                            momentum.initialVelocityStepsPerSecond,
                            tuning.momentumDecayMilliseconds,
                            tuning.maximumCoastFraction,
                            diagnostics.starvationEvents);
                    }
#else
                    // No reference remains. Append a single smooth braking
                    // tail behind already queued motion, then stop generating.
                    drainingToHold = true;
#endif
                }

#ifdef OSSM_STREAM_TUNING
                const uint64_t syntheticBufferTarget =
                    static_cast<uint64_t>(
                        executionHorizonMilliseconds(tuning)) *
                    kTicksPerMillisecond;
                while (momentum.active && planner.canBufferWaypoint() &&
                       planner.bufferedTicks() < syntheticBufferTarget) {
                    if (!appendMomentumWaypoint(planner, momentum, legalRange,
                                                tuning, limits))
                        break;
                }
                hasWaypoints = planner.hasWaypoint();
                if (starvationActive && !momentum.active && !hasWaypoints &&
                    !planner.isStationary())
                    drainingToHold = true;
#endif

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

                    const uint32_t primeMs =
                        requiredPrimeMilliseconds(tuning);
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
                           executionHorizonMilliseconds(tuning) *
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
                       executionHorizonMilliseconds(tuning) *
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
            communication_priority::setStreamingActive(false);
#ifdef OSSM_STREAM_TUNING
            resetTuningParameters();
#endif
            logSummary(diagnostics);
            vTaskDelete(nullptr);
        }

    }  // namespace

    const char *tuningApplyStatusName(TuningApplyStatus status) {
        switch (status) {
            case TuningApplyStatus::Applied:
                return "applied";
            case TuningApplyStatus::Unsupported:
                return "unsupported";
            case TuningApplyStatus::Invalid:
                return "invalid";
            case TuningApplyStatus::NotStreamingIdle:
                return "not_streaming_idle";
            case TuningApplyStatus::SpeedNotZero:
                return "speed_not_zero";
            case TuningApplyStatus::InputQueueNotEmpty:
                return "input_queue_not_empty";
            case TuningApplyStatus::StepperQueueNotEmpty:
                return "stepper_queue_not_empty";
        }
        return "unknown";
    }

    TuningSnapshot tuningSnapshot() {
        TuningSnapshot snapshot{};
#ifdef OSSM_STREAM_TUNING
        portENTER_CRITICAL(&tuningMux);
        snapshot.parameters = activeTuning;
        snapshot.revision = tuningRevision;
        portEXIT_CRITICAL(&tuningMux);
        snapshot.hash =
            timed_streaming::tuningParametersHash(snapshot.parameters);
        snapshot.supported = true;
#endif
        return snapshot;
    }

    TuningApplyStatus applyTuningParameters(
        const timed_streaming::TuningParameters &parameters) {
#ifndef OSSM_STREAM_TUNING
        (void)parameters;
        return TuningApplyStatus::Unsupported;
#else
        if (timed_streaming::validateTuningParameters(parameters) !=
            timed_streaming::TuningValidationError::None)
            return TuningApplyStatus::Invalid;
        if (stateMachine == nullptr ||
            !stateMachine->is("streaming.idle"_s))
            return TuningApplyStatus::NotStreamingIdle;
        if (settings.speed != 0 || settings.speedBLE.value_or(0) != 0)
            return TuningApplyStatus::SpeedNotZero;
        if (targetQueueSize() != 0)
            return TuningApplyStatus::InputQueueNotEmpty;
        if (stepper == nullptr || stepper->isRunning() ||
            !stepper->isQueueEmpty())
            return TuningApplyStatus::StepperQueueNotEmpty;

        portENTER_CRITICAL(&tuningMux);
        activeTuning = parameters;
        ++tuningRevision;
        const uint32_t revision = tuningRevision;
        portEXIT_CRITICAL(&tuningMux);
        ESP_LOGI(
            "Streaming",
            "STREAM_TUNING event=applied revision=%u hash=%llu jerk_ms=%u "
            "prime_ms=%u horizon_ms=%u accel_scale=%.4f decay_ms=%u "
            "max_coast=%.4f edge_strength=%.4f center_strength=%.4f",
            revision,
            static_cast<unsigned long long>(
                timed_streaming::tuningParametersHash(parameters)),
            parameters.jerkRampMilliseconds, parameters.primeMilliseconds,
            parameters.executionHorizonMilliseconds,
            parameters.accelerationScale,
            parameters.momentumDecayMilliseconds,
            parameters.maximumCoastFraction,
            parameters.edgeRepulsionStrength,
            parameters.centerSpringStrength);
        return TuningApplyStatus::Applied;
#endif
    }

    void resetTuningParameters() {
#ifdef OSSM_STREAM_TUNING
        const timed_streaming::TuningParameters defaults{};
        portENTER_CRITICAL(&tuningMux);
        const bool changed =
            !timed_streaming::tuningParametersEqual(activeTuning, defaults);
        activeTuning = defaults;
        if (changed) ++tuningRevision;
        const uint32_t revision = tuningRevision;
        portEXIT_CRITICAL(&tuningMux);
        if (changed)
            ESP_LOGI("Streaming",
                     "STREAM_TUNING event=reset revision=%u hash=%llu",
                     revision,
                     static_cast<unsigned long long>(
                         timed_streaming::tuningParametersHash(defaults)));
#endif
    }

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
        communication_priority::setStreamingActive(true);
        clearTargetQueue();
        immediateStopRequested.store(false, std::memory_order_release);
        immediateStopMicros.store(0, std::memory_order_release);
        // The planner owns about 1 KiB of fixed waypoint storage. A 7.5 KiB
        // task stack was unnecessary and could fail after Wi-Fi/TLS heap
        // fragmentation, leaving the UI in streaming.idle with no consumer
        // for incoming targets. Keep enough margin for planner/FAS call
        // frames, and fail stopped instead of silently accepting commands.
        constexpr int stackSize = 5 * configMINIMAL_STACK_SIZE;
        const BaseType_t created = xTaskCreatePinnedToCore(
            startStreamingTask, "startStreamingTask", stackSize, nullptr,
            configMAX_PRIORITIES - 1, nullptr, Tasks::operationTaskCore);
        if (created != pdPASS) {
            communication_priority::setStreamingActive(false);
            forceSpeedZero();
            clearTargetQueue();
            ESP_LOGE("Streaming",
                     "STREAM_ERROR type=task_start stack_bytes=%d",
                     stackSize);
        }
    }

}  // namespace streaming
