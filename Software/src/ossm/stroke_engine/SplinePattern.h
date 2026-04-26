#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include "spline.h"

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
    bool loadFromFile(const char* id, float playRangeMm,
                      float maxSpeedMmPerSec);
    bool loadFromJson(const char* id, const char* jsonText, float playRangeMm,
                      float maxSpeedMmPerSec);

    SplineSample evaluate(double t);

    float totalDuration() const { return _totalDuration; }
    const char* name() const { return _name; }
    size_t pointCount() const { return _spline.get_x().size(); }
    double getTimeOffset() const { return timeOffset; }

  private:
    tk::spline _spline;
    size_t _pointCount = 0;
    float _totalDuration = 0.0f;
    char _name[64] = {};

    double lastSpeedPercent = 0;
    double timeOffset = 0;

    long startTime = 0;
    long timeDelta = 0;
    long currentSplinePercent = 0;
};

#endif  // OSSM_SPLINE_PATTERN_H
