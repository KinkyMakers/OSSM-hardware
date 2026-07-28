# Timed Streaming Validation

## 2026-07-29 optimization run

This report separates physical evidence from deterministic planner evidence.
Stepper-counter position is useful for transport, tracking, dropout recovery,
and hard-range validation. It is not a substitute for 240 Hz camera tracking
when qualifying mechanical acceleration or jerk.

### Acceptance priorities

1. Never leave the requested play range.
2. On input starvation, brake with bounded jerk and return near the center of
   the play range; never snap to a stop unless continued motion would leave the
   range.
3. Prefer smooth approximate motion over exact zero-lag position tracking.
4. Penalize tracking error outside 10% of play-range width, after horizontal
   lag and vertical translation alignment.
5. Use `FastAccelStepper::moveTimed()` for streaming output. Do not use the
   trapezoidal motion planner for the streaming trajectory.

### Firmware under test

- Branch: `AJ/streaming-optimization`
- Core implementation: `8e7eb3f`
- Streaming task allocation fail-safe: `b94332d`
- Homing task allocation and timeout fail-safe: `a54fcbd`
- Corrected high-rate analyzer: `906a4b4`
- Planner metrics: `ec8f9f9`
- Observable hardware-test startup: `aae3d96`
- FastAccelStepper: `1.2.7`, with local `moveTimed()` recovery patch
- Upstream manifest checked 2026-07-29: `master` also declares version `1.2.7`
  (`https://raw.githubusercontent.com/gin66/FastAccelStepper/master/library.properties`)
- Streaming output: 4 ms timed slices and a 40 ms acceleration ramp

The installed bench firmware includes the core streaming implementation through
`b94332d`. The later homing fail-safe is built but is not installed because
flashing would reboot and home the currently unhomed mechanism without an
available camera gate.

### Physical counter suites

All counter suites used the actual Awesome over BLE and its
FastAccelStepper-reported position. The configured rail was 138.55 mm. Raw
physical position, without lag or vertical fitting, was used for rail safety.

| Suite | Commands | ACK lateness p95 | Aligned RMSE | Lag | Minimum raw rail margin | Dropout-center error | Raw rail violation |
|---|---:|---:|---:|---:|---:|---:|---:|
| Sine, repeated-position keepalives | 41/41 | 0.078 ms | 2.064 mm | 410 ms | 28.45 mm | n/a | 0 mm |
| Sine, 100 ms buffer and latency compensation | 41/41 | 0.083 ms | 2.142 mm | 445 ms | 28.00 mm | n/a | 0 mm |
| Triangle with delivery jitter | 41/41 | 0.090 ms | 2.238 mm | 450 ms | 31.95 mm | n/a | 0 mm |
| Sine with an intentional packet gap | 30/30 sent | 0.058 ms | not comparable across dropout | not scored | 28.55 mm | 0.412 mm | 0 mm |

The dropout-center tolerance was 13.855 mm, or 10% of rail width. The measured
0.412 mm error was inside that tolerance. The buffered sine eliminated the
mid-run braking-tail/data-resume cycles seen without lookahead. Each suite ended
with one intentional starvation and smooth center recovery.

The Essential State characteristic updated physical position at approximately
4 Hz. Consequently, acceleration, jerk, and high-frequency-power values derived
from these counter samples are explicitly marked unqualified. They must not be
used as mechanical smoothness acceptance evidence.

Local artifacts:

- `/Users/aj/Movies/R+D Motion Lab/ossm-counter-regular-keepalive-20260729`
- `/Users/aj/Movies/R+D Motion Lab/ossm-counter-regular-buffered-20260729`
- `/Users/aj/Movies/R+D Motion Lab/ossm-counter-suites-20260729-precise`
- `/Users/aj/Library/Logs/R+D Motion Lab/ossm-serial-20260729-024746-E8F85067.log`

The successful serial summaries reported zero retries, underruns, rebuffers,
input overflows, fatal submissions, safety clamps, and reconciliation errors.
Raw stepper position retained at least 557 steps of legal margin.

### Deterministic 4 ms trajectory evidence

The native planner tests observe every 4 ms trajectory slice, independently of
the low-rate BLE counter telemetry.

| Suite | Aligned tracking / smoothness | Maximum acceleration | Maximum jerk | Range result |
|---|---:|---:|---:|---|
| Nominal sine | 1.280 mm RMSE at 30 ms lag | 17,500 mm/s² | 1,250,000 mm/s³ | Within envelope |
| Buffered 20 Hz sine | 0.862873% power above 5 Hz | 8,869 mm/s² | 625,000 mm/s³ | No safety clamp |
| Triangle reversals | jerk-limited at every reversal | 9,019 mm/s² | 1,250,000 mm/s³ | Within envelope |
| Dropout braking and center recovery | 0.25 mm final center error | 10,000 mm/s² | 1,250,000 mm/s³ | 5 mm minimum guard |

These values describe the commanded step trajectory, not measured mechanism
compliance, vibration, belt elasticity, or missed steps. Every slice is asserted
against the configured speed, acceleration, and jerk envelopes.

### Cost and safety semantics

The counter analyzer permits horizontal lag and vertical translation when
scoring requested-versus-measured tracking. It does not permit fitting to hide
a physical rail violation. Cost terms include:

- aligned RMSE and absolute tracking error;
- a significant penalty beyond 10% of play-range width;
- high-frequency velocity power and jerk ratio when derivative data is
  sufficiently sampled;
- dropout distance from the play-range center;
- infinite cost for any raw physical range violation or unsafe dropout result.

Firmware clamps every trajectory endpoint to the legal range and maintains a
stopping-distance envelope near either edge. Empty input produces a single
jerk-limited braking tail. After stationary hold, it performs a bounded smooth
center recovery. New data resumes from the attained state without an immediate
force stop.

### Verification

- Native firmware regression: 238/238 tests passed.
- Timed planner suite: 20/20 tests passed with emitted motion metrics.
- `hw_streaming_quick` image builds successfully and contains four cases:
  nominal sine, jittered triangle, physically possible random motion, and sine
  with delivery gaps.
- AgentTools regression: 46/46 tests passed.
- Firmware tooling now classifies every embedded image without an explicit
  no-servo flag as motion-capable. `hw_streaming_quick` inherits the hardware
  test interlock and refuses to run without `--allow-hardware-motion`.

### Remaining acceptance gate

The current controller does not yet have a valid 240 Hz camera recording. The
camera phone became passcode-locked, and Motion Lab currently reports no active
camera stream. Two earlier 2026-07-29 recordings captured firmware defects and
showed no valid motion, so they are diagnostic-only and are intentionally not
presented as acceptance videos.

The next physical run must use one fresh inspected pre-run frame, with no
repeated tag-visibility preflight, then:

1. Install the source containing the homing allocation fail-safe.
2. Capture the new boot serial log and verify homing completes.
3. Record the four-case quick suite at 240 Hz with tag 5 trajectory tracking.
4. Validate monotonic PTS, zero decoder errors, camera frame rate and coverage,
   raw rail containment, tracking alignment, acceleration, jerk, and dropout
   center recovery.
5. Capture one inspected post-run frame and leave the device stopped,
   returned to menu, and disconnected.

Until that recording passes, physical tracking and range behavior are verified,
but medical-contact mechanical smoothness remains unqualified.
