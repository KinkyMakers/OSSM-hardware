#ifndef OSSM_STREAMING_H
#define OSSM_STREAMING_H

namespace streaming {

    /**
     * Start the streaming motion task
     * Receives position/time commands over BLE and follows them
     */
    void startStreaming();

    // Stops Streaming's timed queue immediately without disabling the motor.
    // Safe to call from the BLE command task; it is a no-op for other modes.
    void requestImmediateStop();

}  // namespace simple_penetration

#endif  // OSSM_STREAMING_H
