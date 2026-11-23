#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

// Smooth path through the queued-move keyframes ("q:" command), so the
// carriage flows through each point instead of stopping at it.
// No hardware dependencies — testable on native platform.
//
// The path is a cubic Hermite curve through keyframes (timeMs, position).
// Each keyframe's slope comes from its neighbours (monotone, Fritsch-Carlson
// style), so the curve:
//   - passes through every keyframe at its time,
//   - never goes beyond the range of the two keyframes around it (no
//     overshoot past what the client sent, so it stays inside the stroke
//     window),
//   - has slope 0 at a turning point and at both ends: when the buffer runs
//     dry the carriage eases to a stop on the last known point.

namespace trajectory {

constexpr size_t kMaxKeyframes = 10;

struct Keyframe {
    uint32_t timeMs;  // millis(); differences are taken as int32 to survive wrap
    float position;   // stepper steps
};

/// Slope at a keyframe from the secant slopes either side of it (d0 before,
/// d1 after) and the interval lengths (h0 before, h1 after). 0 at a turning
/// point or a flat spot; otherwise a weighted harmonic mean of the secants
/// (Brodlie), which keeps each segment monotone.
inline float monotoneSlope(float d0, float d1, float h0, float h1) {
    if (d0 * d1 <= 0.0f) {
        return 0.0f;
    }
    const float w0 = 2.0f * h1 + h0;
    const float w1 = h1 + 2.0f * h0;
    return (w0 + w1) / (w0 / d0 + w1 / d1);
}

/// Cubic Hermite between p0 (slope m0) and p1 (slope m1) over an interval
/// of length h, at s in [0, 1]. Slopes are per unit time.
inline float hermite(float p0, float p1, float m0, float m1, float h,
                     float s) {
    const float s2 = s * s;
    const float s3 = s2 * s;
    return (2 * s3 - 3 * s2 + 1) * p0 + (s3 - 2 * s2 + s) * h * m0 +
           (-2 * s3 + 3 * s2) * p1 + (s3 - s2) * h * m1;
}

class Trajectory {
  public:
    void clear() { count_ = 0; }
    size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    const Keyframe &operator[](size_t i) const { return keys_[i]; }
    const Keyframe &back() const { return keys_[count_ - 1]; }

    /// Append a keyframe. Times must not go backwards: one at the same time
    /// as the last (a 0 ms move) replaces its position; an earlier one is
    /// ignored. Returns false when full or ignored.
    bool push(uint32_t timeMs, float position) {
        if (count_ > 0) {
            const int32_t dt = static_cast<int32_t>(timeMs - back().timeMs);
            if (dt < 0) {
                return false;
            }
            if (dt == 0) {
                keys_[count_ - 1].position = position;
                return true;
            }
        }
        if (count_ == kMaxKeyframes) {
            return false;
        }
        keys_[count_++] = {timeMs, position};
        return true;
    }

    /// Drop the oldest keyframes, keeping the newest `keep`.
    void keepLast(size_t keep) {
        if (count_ <= keep) {
            return;
        }
        const size_t drop = count_ - keep;
        for (size_t i = 0; i < keep; ++i) {
            keys_[i] = keys_[i + drop];
        }
        count_ = keep;
    }

    /// Position on the curve at timeMs. Before the first keyframe it holds the
    /// first position, after the last it holds the last. Empty: 0.
    float positionAt(uint32_t timeMs) const {
        if (count_ == 0) {
            return 0.0f;
        }
        if (static_cast<int32_t>(timeMs - keys_[0].timeMs) <= 0) {
            return keys_[0].position;
        }
        if (static_cast<int32_t>(timeMs - back().timeMs) >= 0) {
            return back().position;
        }

        size_t i = 0;
        while (static_cast<int32_t>(timeMs - keys_[i + 1].timeMs) >= 0) {
            ++i;
        }

        const float h = interval(i);
        const float s =
            static_cast<float>(static_cast<int32_t>(timeMs - keys_[i].timeMs)) /
            h;
        return hermite(keys_[i].position, keys_[i + 1].position, slope(i),
                       slope(i + 1), h, s);
    }

  private:
    // Length of the interval from keyframe i to i+1, in ms (> 0 by push).
    float interval(size_t i) const {
        return static_cast<float>(
            static_cast<int32_t>(keys_[i + 1].timeMs - keys_[i].timeMs));
    }

    float secant(size_t i) const {
        return (keys_[i + 1].position - keys_[i].position) / interval(i);
    }

    // Slope at keyframe i, in steps per ms. The ends get 0: the start is
    // history and the end is where the carriage stops if nothing follows.
    float slope(size_t i) const {
        if (i == 0 || i + 1 >= count_) {
            return 0.0f;
        }
        return monotoneSlope(secant(i - 1), secant(i), interval(i - 1),
                             interval(i));
    }

    Keyframe keys_[kMaxKeyframes] = {};
    size_t count_ = 0;
};

constexpr uint32_t kMinLookaheadMs = 20;
constexpr uint32_t kMaxLookaheadMs = 150;
// Aim at least this many steps ahead. Targets are whole steps, so aiming a
// step or two ahead makes the target, and the speed worked out from it, jump
// between neighbouring steps from tick to tick: the motor stutters.
constexpr float kMinAimSteps = 8.0f;

/// How far ahead on the curve to aim, from the carriage's speed. The longer
/// of:
///   - braking: the motor driver brakes once its stopping distance v^2 / 2a
///     reaches the distance left to the target; aiming v * h ahead keeps it
///     from braking mid-stroke when h > v / 2a (with a 1.5x margin), and
///   - resolution: at speed v, h = kMinAimSteps / v keeps the aim several
///     steps ahead as it slows, so step rounding stays a small fraction.
/// Clamped so peaks aren't rounded off more than necessary; standing still
/// it is the longest.
inline uint32_t lookaheadMs(float speedStepsPerS, float accelStepsPerS2) {
    const float speed = std::fabs(speedStepsPerS);
    if (accelStepsPerS2 <= 0.0f || speed <= 0.0f) {
        return kMaxLookaheadMs;
    }
    const float brakingMs = 1.5f * speed / (2.0f * accelStepsPerS2) * 1000.0f;
    const float resolutionMs = kMinAimSteps / speed * 1000.0f;
    const float ms = std::fmax(brakingMs, resolutionMs);
    if (ms <= kMinLookaheadMs) {
        return kMinLookaheadMs;
    }
    if (ms >= kMaxLookaheadMs) {
        return kMaxLookaheadMs;
    }
    return static_cast<uint32_t>(std::ceil(ms));
}

/// Speed (steps/s) that covers `distanceSteps` in `lookaheadMs`, between
/// minSpeed and maxSpeed. Pass the unrounded distance to the aim point, so the
/// speed doesn't jump with step rounding.
inline uint32_t pursuitSpeed(float distanceSteps, uint32_t lookahead,
                             uint32_t minSpeed, uint32_t maxSpeed) {
    const float speed =
        std::fabs(distanceSteps) * 1000.0f / static_cast<float>(lookahead);
    if (speed <= minSpeed) {
        return minSpeed;
    }
    if (speed >= maxSpeed) {
        return maxSpeed;
    }
    return static_cast<uint32_t>(speed);
}

}  // namespace trajectory
