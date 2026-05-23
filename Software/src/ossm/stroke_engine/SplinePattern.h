#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include <tinyspline.h>

#include <vector>

struct SplineSample {
    double t;
    double position;
    double velocity;
    double acceleration;
    double jerk;
    double speedPercent;
};

class SplinePattern {
  public:
    SplinePattern();
    ~SplinePattern();

    // Non-copyable. The owned tsBSpline handles wrap raw heap allocations
    // managed by tinyspline; copying them would lead to double-free on
    // destruction. Move semantics are not needed for the current call sites.
    SplinePattern(const SplinePattern&) = delete;
    SplinePattern& operator=(const SplinePattern&) = delete;

    bool loadFromFile(const char* id, float playRangeMm,
                      float maxSpeedMmPerSec, float maxAccelMmPerSec2,
                      float maxJerkMmPerSec3);
    bool loadFromJson(const char* id, const char* jsonText, float playRangeMm,
                      float maxSpeedMmPerSec, float maxAccelMmPerSec2,
                      float maxJerkMmPerSec3);

    // Raw sample using the slope-only `_totalDuration` (today's behaviour).
    // Position/velocity/acceleration may exceed the device's physical
    // limits; callers that drive the motor should prefer evaluateFeasible.
    SplineSample evaluate(double speedPercent);

    // Sample using `_feasibleTotalDuration`, the period stretched far enough
    // that |velocity|, |acceleration|, and |jerk| stay within the limits
    // supplied at load time (when expressed in physical units via
    // playRangeMm). At speedPercent <= 1.0 the returned sample is
    // physically realisable.
    SplineSample evaluateFeasible(double speedPercent);

    float totalDuration() const { return _totalDuration; }
    float feasibleTotalDuration() const { return _feasibleTotalDuration; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _baseAnchorCount; }
    double getTimeOffset() const { return timeOffset; }

  private:
    // Looks up an anchor [x_i, x_{i+1}) interval inside the tiled spline and
    // remembers the matching u-domain in the underlying tsBSpline. Each
    // tsBSpline of type TS_BEZIERS is parameterised over [0, 1] split into
    // tiledSegmentCount equal-length u-windows, one per cubic Bezier segment.
    struct TiledSegment {
        double xStart;
        double xEnd;
        double uStart;
        double uEnd;
    };

    // 2D Bezier B-spline over (x, y). x(u) is the time axis, y(u) is the
    // normalised position. Derivative splines are pre-built once at load to
    // avoid per-evaluate allocation.
    tsBSpline _spline;
    tsBSpline _splineDeriv1;
    tsBSpline _splineDeriv2;
    tsBSpline _splineDeriv3;
    bool _splineReady = false;

    std::vector<TiledSegment> _segments;
    size_t _baseAnchorCount = 0;
    // Slope-only period (kept for backwards compatibility with evaluate()).
    float _totalDuration = 0.0f;
    // Period stretched against speed, accel, and jerk simultaneously.
    float _feasibleTotalDuration = 0.0f;
    char _name[64] = {};

    double lastSpeedPercent = 0;
    double timeOffset = 0;
    // evaluateFeasible runs against its own time-base, so it needs an
    // independent speed-change rebasing offset to avoid jumping when the
    // user changes speed mid-stroke.
    double lastSpeedPercentFeasible = 0;
    double timeOffsetFeasible = 0;

    long startTime = 0;
    long timeDelta = 0;
    long currentSplinePercent = 0;

    void freeSplines();

    // Shared core for evaluate() / evaluateFeasible(): drives the time-base
    // off `period` (seconds for one full pattern) and the per-evaluator
    // speed-tracking state. Computes jerk only when computeJerk is true.
    SplineSample sampleAtPeriod(double speedPercent, float period,
                                double& lastSpeed, double& tOffset,
                                bool computeJerk);
};

#endif  // OSSM_SPLINE_PATTERN_H
