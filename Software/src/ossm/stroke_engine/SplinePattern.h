#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include <tinyspline.h>

struct SplineSample {
    double t;
    double position;
    double velocity;
    double acceleration;
    double speedPercent;
    // Normalised stroke position along the pattern y-axis, clamped to [0, 1].
    double handleY;
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
                      float maxSpeedMmPerSec);
    bool loadFromJson(const char* id, const char* jsonText, float playRangeMm,
                      float maxSpeedMmPerSec);

    // Sample using the slope-only `_totalDuration`. The period is sized so a
    // pattern played at speedPercent = 1.0 reaches |dy/dx|·playRangeMm =
    // maxSpeedMmPerSec at its steepest point; callers are still responsible
    // for clamping the returned velocity / acceleration to the device's
    // physical limits when driving the motor.
    SplineSample evaluate(double speedPercent);

    float totalDuration() const { return _totalDuration; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _baseAnchorCount; }
    double getTimeOffset() const { return timeOffset; }

  private:
    // 2D Bezier B-spline over (t, y). t is the pattern time axis in [0, 1]
    // per period; y is the normalised position. Derivative splines are
    // pre-built once at load to avoid per-evaluate allocation. The spline is
    // constructed from a tiled anchor sequence (3 copies of one period + a
    // closing anchor) so the wrap-around is smooth; uniform TS_BEZIERS knots
    // mean segment i spans knot t ∈ [i / N, (i+1) / N] where N =
    // num_control_points / 4. One period maps to the middle tile only:
    // [_knotTMin, _knotTMax], not the full [0, 1] knot range.
    tsBSpline _spline;
    tsBSpline _splineDeriv1;
    tsBSpline _splineDeriv2;
    bool _splineReady = false;

    size_t _baseAnchorCount = 0;
    double _knotTMin = 0.0;
    double _knotTMax = 1.0;
    float _totalDuration = 0.0f;
    char _name[64] = {};

    double lastSpeedPercent = 0;
    double timeOffset = 0;

    long startTime = 0;
    long timeDelta = 0;
    long currentSplinePercent = 0;

    void freeSplines();

    // Shared core for evaluate(): drives the time-base off `period` (seconds
    // for one full pattern) and the speed-tracking state.
    SplineSample sampleAtPeriod(double speedPercent, float period,
                                double& lastSpeed, double& tOffset);
};

#endif  // OSSM_SPLINE_PATTERN_H
