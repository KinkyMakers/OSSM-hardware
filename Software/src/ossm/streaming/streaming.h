#ifndef OSSM_STREAMING_H
#define OSSM_STREAMING_H

#include <cstdint>

#include "timed_streaming_tuning.h"

namespace streaming {

    enum class TuningApplyStatus {
        Applied,
        Unsupported,
        Invalid,
        NotStreamingIdle,
        SpeedNotZero,
        InputQueueNotEmpty,
        StepperQueueNotEmpty,
    };

    struct TuningSnapshot {
        timed_streaming::TuningParameters parameters{};
        uint32_t revision = 0;
        uint64_t hash = 0;
        bool supported = false;
    };

    /**
     * Start the streaming motion task
     * Receives position/time commands over BLE and follows them
     */
    void startStreaming();

    // Stops Streaming's timed queue immediately without disabling the motor.
    // Safe to call from the BLE command task; it is a no-op for other modes.
    void requestImmediateStop();

    TuningSnapshot tuningSnapshot();
    TuningApplyStatus applyTuningParameters(
        const timed_streaming::TuningParameters &parameters);
    void resetTuningParameters();
    const char *tuningApplyStatusName(TuningApplyStatus status);

}  // namespace simple_penetration

#endif  // OSSM_STREAMING_H
