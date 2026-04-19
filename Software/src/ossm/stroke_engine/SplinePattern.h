#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include <optional>
#include <vector>

struct SplineHandle {
    float x;
    float y;
};

struct SplinePoint {
    float x, y;
    std::optional<SplineHandle> handleIn;
    std::optional<SplineHandle> handleOut;
};

class SplinePattern {
  public:
    bool loadFromFile(const char* id, float playRangeMm,
                      float maxSpeedMmPerSec);

    float evaluate(float t) const;
    float evaluateVelocity(float t) const;

    float totalDuration() const { return _totalDuration; }
    float minSegmentDx() const { return _minDx; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _points.size(); }

  private:
    std::vector<SplinePoint> _points;
    float _totalDuration = 0;
    float _minDx = 0;
    char _name[64] = {};

    int findSegment(float t) const;

    static float hermite(float dt, const SplinePoint& p1,
                         const SplinePoint& p2);
    static float hermiteDerivative(float dt, const SplinePoint& p1,
                                   const SplinePoint& p2);
};

#endif  // OSSM_SPLINE_PATTERN_H
