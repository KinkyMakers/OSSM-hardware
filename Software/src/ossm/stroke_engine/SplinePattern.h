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

    // Position-based, closed-loop sample. Given where the handle actually is
    // (currentPosition, normalised [0, 1]) and which way it is moving
    // (direction: true = forward / inserting / dy/dt > 0), first re-sync to the
    // matching point on the curve, then advance by currentSpeed (0..1) over
    // timeStepMs and sample the next target. Callers remain responsible for
    // clamping the returned velocity / acceleration to the device's physical
    // limits when driving the motor.
    SplineSample evaluate(float currentPosition, float currentSpeed,
                          bool direction, float timeStepMs);

    float totalDuration() const { return _totalDuration; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _baseAnchorCount; }
    double lastPeriodT() const { return _lastPeriodT; }

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

    // Last synced position on the curve, in normalised period units [0, 1).
    // The forward re-sync scan in evaluate() starts here each tick.
    double _lastPeriodT = 0.0;

    void freeSplines();

    // Forward-scan (with looping) from `_lastPeriodT` to find where the handle
    // currently sits on the curve. Returns the matching normalised period t in
    // [0, 1): the nearest point ahead whose y is within epsilon of
    // currentPosition and whose dy/dt sign matches `direction`, or the globally
    // closest y (ignoring direction) if no such point exists.
    double syncToPosition(float currentPosition, bool direction);
};

#endif  // OSSM_SPLINE_PATTERN_H
