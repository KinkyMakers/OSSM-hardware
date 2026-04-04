#ifndef OSSM_SPLINE_PATTERN_H
#define OSSM_SPLINE_PATTERN_H

#include <vector>

struct SplinePoint {
    float x, y;
    float handleInX, handleInY;
    float handleOutX, handleOutY;
};

class SplinePattern {
   public:
    bool loadFromFile(const char* id);

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

    static float hermite(float dt, float y0, float y1, float m0, float m1);
    static float hermiteDerivative(float dt, float y0, float y1, float m0,
                                   float m1, float segmentDx);
};

#endif  // OSSM_SPLINE_PATTERN_H
