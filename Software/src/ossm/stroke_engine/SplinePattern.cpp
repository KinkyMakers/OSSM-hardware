#include "SplinePattern.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>

static const char* TAG = "SplinePattern";

bool SplinePattern::loadFromFile(const char* id, float playRangeMm,
                                 float maxSpeedMmPerSec) {
    char path[128];
    snprintf(path, sizeof(path), "/littlefs/patterns/%s.json", id);

    FILE* f = fopen(path, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open %s", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc(size + 1);
    if (!buf) {
        fclose(f);
        ESP_LOGE(TAG, "Failed to allocate %ld bytes for %s", size, path);
        return false;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, buf);
    free(buf);

    if (error) {
        ESP_LOGE(TAG, "JSON parse failed for %s: %s", path, error.c_str());
        return false;
    }

    const char* patternName = doc["name"] | "Spline";
    strncpy(_name, patternName, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';

    JsonArray spline = doc["cubicSpline"];
    if (spline.isNull() || spline.size() < 2) {
        ESP_LOGE(TAG, "cubicSpline missing or too short in %s", path);
        return false;
    }

    _points.clear();
    _points.reserve(spline.size());

    for (JsonObject pt : spline) {
        SplinePoint sp;
        sp.x = pt["x"] | 0.0f;
        sp.y = pt["y"] | 0.0f;

        auto hIn = pt["handleIn"];
        auto hOut = pt["handleOut"];
        if (!hIn.isNull()) {
            sp.handleIn = SplineHandle{hIn["x"] | 0.0f, hIn["y"] | 0.0f};
        }
        if (!hOut.isNull()) {
            sp.handleOut = SplineHandle{hOut["x"] | 0.0f, hOut["y"] | 0.0f};
        }

        _points.push_back(sp);
    }

    // Fill missing handles using half-chord offsets to the neighbor.
    // handleIn of first point and handleOut of last point are unused by
    // evaluateVelocity, so they stay nullopt if missing.
    for (size_t i = 0; i + 1 < _points.size(); ++i) {
        SplinePoint& p0 = _points[i];
        SplinePoint& p1 = _points[i + 1];
        const float halfDx = (p1.x - p0.x) * 0.5f;
        const float halfDy = (p1.y - p0.y) * 0.5f;

        if (!p0.handleOut) {
            p0.handleOut = SplineHandle{halfDx, halfDy};
        }
        if (!p1.handleIn) {
            p1.handleIn = SplineHandle{-halfDx, -halfDy};
        }
    }

    _totalDuration = _points.back().x;

    // Assume an ordered list
    // find the min distance between adjacent points to set the scaling factor.

    float _maxSlopeAbs = 0;
    float _dxAtMaxSlopePercent = 0;
    for (size_t i = 1; i < _points.size(); i++) {
        float dx = _points[i].x - _points[i - 1].x;
        if (dx < 0.0001f) continue;
        float slope = fabsf((_points[i].y - _points[i - 1].y) / dx);
        if (slope > _maxSlopeAbs) {
            _maxSlopeAbs = slope;
            _dxAtMaxSlopePercent = dx;
        }
    }
    float _dxAtMaxSlopeSeconds = playRangeMm / maxSpeedMmPerSec;

    ESP_LOGI(TAG,
             "maxSlopeAbs: %.3f, dxAtMaxSlopePercent: %.3f, "
             "dxAtMaxSlopeSeconds: %.3f",
             _maxSlopeAbs, _dxAtMaxSlopePercent, _dxAtMaxSlopeSeconds);

    _totalDuration = _dxAtMaxSlopeSeconds / _dxAtMaxSlopePercent;

    ESP_LOGI(TAG, "Loaded '%s': %d points, duration=%.3f, minDx=%.3f", _name,
             _points.size(), _totalDuration, _minDx);

    return true;
}

int SplinePattern::findSegment(float t) const {
    if (_points.size() < 2) return 0;

    for (size_t i = 1; i < _points.size(); i++) {
        if (t <= _points[i].x) {
            return i - 1;
        }
    }

    return _points.size() - 2;
}

float SplinePattern::hermite(float dt, const SplinePoint& p1,
                             const SplinePoint& p2) {
    float deltaX = p2.x - p1.x;
    float deltaY = p2.y - p1.y;
    float m0 = deltaY / deltaX;
    float m1 = 0;
    float m2 = 0;
    float a = (m1 + m2 - 2.0f * m0) / (deltaX * deltaX);
    float b = (m2 - m1) / (2.0f * deltaX) - (3.0f / 2.0f) * (p1.x + p2.x) * a;
    float c = m1 - 3.0f * (p1.x * p1.x) * a - 2.0f * b * p1.x;
    float d = p1.y - a * p1.x * p1.x * p1.x - b * p1.x * p1.x - c * p1.x;

    return a * dt * dt * dt + b * dt * dt + c * dt + d;
}

float SplinePattern::hermiteDerivative(float dt, const SplinePoint& p1,
                                       const SplinePoint& p2) {
    float deltaX = p2.x - p1.x;
    float deltaY = p2.y - p1.y;
    float m0 = deltaY / deltaX;
    float m1 = 0;
    float m2 = 0;
    float a = (m1 + m2 - 2.0f * m0) / (deltaX * deltaX);
    float b = (m2 - m1) / (2.0f * deltaX) - (3.0f / 2.0f) * (p1.x + p2.x) * a;
    float c = m1 - 3.0f * (p1.x * p1.x) * a - 2.0f * b * p1.x;
    float d = p1.y - a * p1.x * p1.x * p1.x - b * p1.x * p1.x - c * p1.x;

    return 3.0f * a * dt * dt + 2.0f * b * dt + c;
}

float SplinePattern::evaluate(float t) const {
    if (_points.empty()) return 0;
    if (_points.size() == 1) return _points[0].y;

    // if (_totalDuration > 0) {
    //     t = fmodf(t, _totalDuration);
    //     if (t < 0) t += _totalDuration;
    // }

    int seg = findSegment(t);
    const SplinePoint& p0 = _points[seg];
    const SplinePoint& p1 = _points[seg + 1];

    return hermite(t, p0, p1);
}

float SplinePattern::evaluateVelocity(float t) const {
    if (_points.size() < 2) return 0;

    if (_totalDuration > 0) {
        t = fmodf(t, _totalDuration);
        if (t < 0) t += _totalDuration;
    }

    int seg = findSegment(t);
    const SplinePoint& p0 = _points[seg];
    const SplinePoint& p1 = _points[seg + 1];

    return hermiteDerivative(t, p0, p1);
}
