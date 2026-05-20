#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include <tinyspline.h>

#include <vector>

struct SplineSample {
    double t;
    double position;
    double velocity;
    double acceleration;
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
                      float maxSpeedMmPerSec);
    bool loadFromJson(const char* id, const char* jsonText, float playRangeMm,
                      float maxSpeedMmPerSec);

    SplineSample evaluate(double speedPercent);

    float totalDuration() const { return _totalDuration; }
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
    bool _splineReady = false;

    std::vector<TiledSegment> _segments;
    size_t _baseAnchorCount = 0;
    float _totalDuration = 0.0f;
    char _name[64] = {};

    double lastSpeedPercent = 0;
    double timeOffset = 0;

    long startTime = 0;
    long timeDelta = 0;
    long currentSplinePercent = 0;

    void freeSplines();
};

#endif  // OSSM_SPLINE_PATTERN_H
