#ifndef OSSM_STREAMING_H
#define OSSM_STREAMING_H

#include <cstddef>
#include <cstdint>

namespace streaming {

    /**
     * Start the streaming motion task
     * Receives position/time commands over BLE and follows them
     */
    void startStreaming();

    /**
     * Handle a binary "q:" queued-moves command (see queued_moves.h).
     * Safe to call from the BLE task. Returns false, changing nothing, when
     * the command is malformed, streaming mode isn't running, or the moves
     * don't all fit in the buffer. "q:" with no moves clears the buffer and
     * stops all streaming motion.
     */
    bool handleQueueCommand(const uint8_t *data, size_t length);

    /**
     * Handle a binary "r:" command: set the range q:/r: position bytes are
     * decoded in, and queue the moves that follow it (see queued_moves.h).
     * Safe to call from the BLE task. Returns false, changing nothing (range
     * included), when the command is malformed, streaming mode isn't running,
     * or the moves don't all fit in the buffer.
     */
    bool handleRangeCommand(const uint8_t *data, size_t length);

    /** set:lowwater:n — ms of queued moves to aim to keep (default 500). */
    void setLowWaterMs(uint32_t ms);

    /**
     * The status packet's low-water byte: 0 when the queued moves (and the
     * rest of the running one) last at least the low-water time, otherwise
     * the number of free buffer slots. 0 outside streaming mode.
     */
    uint8_t lowWaterSlots();

    /**
     * True, once, after a queued move completes with the buffer below the
     * low-water mark. Stays pending until taken.
     */
    bool takeLowWaterEvent();

}  // namespace streaming

#endif  // OSSM_STREAMING_H
