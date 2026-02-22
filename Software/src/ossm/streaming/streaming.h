#ifndef OSSM_STREAMING_H
#define OSSM_STREAMING_H

namespace streaming {

    /**
     * Start the streaming motion task
     * Receives position/time commands over BLE and follows them
     */
    void startStreaming();

}  // namespace simple_penetration

#endif  // OSSM_STREAMING_H
