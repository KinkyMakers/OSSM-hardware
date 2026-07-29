#ifndef OSSM_STREAM_BACKLOG_POLICY_H
#define OSSM_STREAM_BACKLOG_POLICY_H

#include <cstddef>
#include <cstdint>

namespace stream_backlog_policy {

    constexpr uint32_t kMaximumWaypointAgeMilliseconds = 250;
    constexpr uint32_t kMaximumQueuedDurationMilliseconds = 250;

    // Never discard the newest known request. The motion planner remains
    // responsible for applying jerk, acceleration, and play-range limits when
    // it joins the retained trajectory.
    constexpr bool shouldDiscardOldest(
        uint32_t oldestAgeMilliseconds,
        uint32_t queuedDurationMilliseconds, size_t queuedWaypoints,
        uint32_t maximumAgeMilliseconds =
            kMaximumWaypointAgeMilliseconds,
        uint32_t maximumQueuedDurationMilliseconds =
            kMaximumQueuedDurationMilliseconds) {
        return queuedWaypoints > 1 &&
               (oldestAgeMilliseconds > maximumAgeMilliseconds ||
                queuedDurationMilliseconds >
                    maximumQueuedDurationMilliseconds);
    }

}  // namespace stream_backlog_policy

#endif  // OSSM_STREAM_BACKLOG_POLICY_H
