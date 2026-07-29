# Timed Streaming

Streaming retains both existing wire formats: `stream:{position}:{durationMs}`
and the three-byte Fleshlight Launch/FTS characteristic. Both producers write
the same 64-entry static FreeRTOS queue.

The Streaming task follows buffered waypoints sequentially and converts the
feed-forward reference into 4 ms `moveTimed()` slices. Configured speed,
sensation-scaled acceleration, a jerk ramp, and a stopping-distance boundary
envelope limit that reference. Positional lag and vertical offset are measured
and aligned only by the external analysis tooling; the firmware does not apply
proportional position correction. Sub-step position and duration rounding
errors are carried, and a slice is committed only after FastAccelStepper
accepts it.

Every target is clamped into the intersection of the active stroke/depth range
and homed rail. The governor reserves the larger of 1 mm or 10% of span as an
interior guard and applies a stopping-distance velocity envelope at both ends.
A final range clamp is safety-only and emits a diagnostic. If input disappears,
the planner jerk-limits to rest, then returns toward range center at no more
than one quarter of the configured speed. The center recovery takes at least
750 ms and repeats only when residual error exceeds 1 mm. New input is buffered
behind this bounded recovery, so signal resumption does not introduce an
immediate-stop jerk. Live range changes stop the queued path and go through
normal re-priming.

With latency compensation enabled, execution waits for `buffer * 2` ms of
future waypoints, clamped to 8–200 ms. Without compensation it waits for two
slices. Normal builds submit no more than 72 ms to FastAccelStepper at once.
The temporary `OSSM_STREAM_TUNING` build can alter the prime duration,
execution horizon, jerk ramp, acceleration scale, momentum decay, and maximum
coast while stopped; hard range and stopping constraints remain fixed.

`set:speed:0` signals `forceStop()` directly from the BLE command handler,
flushes the timed queue while retaining motor hold, clears older waypoints, and
resynchronizes the planner from the step counter. Every accepted endpoint is
reconciled against `getPositionAfterCommandsCompleted()`; stopped queues are
reconciled against `getCurrentPosition()`. Queue overflow and negative
`moveTimed()` results take the same fail-safe path and force the configured
speed to zero. An underrun retains the unplanned remainder and re-primes before
motion resumes.

FastAccelStepper is pinned to 1.2.7 for its corrected `actual_duration`. Its
timed API otherwise leaves queue writes blocked after `forceStop()`, because
only the trapezoidal ramp filler clears that internal flag. The audited
`patch_fastaccelstepper_1_2_7.py` pre-build patch clears it only when the old
queue is stopped and empty, allowing a later timed test to start without an
unrequested re-arm step.

Serial diagnostics use `STREAM_DIAG`, `STREAM_SLICE`, and `STREAM_ERROR`.
They report accepted slices, retry results, underruns, rebuffering, input
overflow, fatal driver results, maximum absolute timing carry, explicit-stop
latency, current/future reconciliation error, reference error, boundary
envelope and safety-clamp activity, range margin, and center fallback. BLE
telemetry is unchanged.
