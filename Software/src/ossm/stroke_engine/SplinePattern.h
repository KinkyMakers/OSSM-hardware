#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include "spline.h"

struct SplineSample {
    double position;
    double velocity;
    double acceleration;
};

class SplinePattern {
  public:
    bool loadFromFile(const char* id, float playRangeMm,
                      float maxSpeedMmPerSec);

    SplineSample evaluate(double t) const;

    float totalDuration() const { return _totalDuration; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _spline.get_x().size(); }

  private:
    tk::spline _spline;
    size_t _pointCount = 0;
    float _totalDuration = 0.0f;
    char _name[64] = {};
};

#endif  // OSSM_SPLINE_PATTERN_H
