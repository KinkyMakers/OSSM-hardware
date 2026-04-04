#include "SplinePattern.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>

static const char* TAG = "SplinePattern";

bool SplinePattern::loadFromFile(const char* id) {
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
        sp.handleInX = pt["handleIn"]["x"] | 0.0f;
        sp.handleInY = pt["handleIn"]["y"] | 0.0f;
        sp.handleOutX = pt["handleOut"]["x"] | 0.0f;
        sp.handleOutY = pt["handleOut"]["y"] | 0.0f;
        _points.push_back(sp);
    }

    _totalDuration = _points.back().x;

    _minDx = _totalDuration;
    for (size_t i = 1; i < _points.size(); i++) {
        float dx = _points[i].x - _points[i - 1].x;
        if (dx > 0 && dx < _minDx) {
            _minDx = dx;
        }
    }

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

float SplinePattern::hermite(float dt, float y0, float y1, float m0,
                             float m1) {
    float dt2 = dt * dt;
    float dt3 = dt2 * dt;

    return (2 * dt3 - 3 * dt2 + 1) * y0 + (dt3 - 2 * dt2 + dt) * m0 +
           (-2 * dt3 + 3 * dt2) * y1 + (dt3 - dt2) * m1;
}

float SplinePattern::hermiteDerivative(float dt, float y0, float y1, float m0,
                                       float m1, float segmentDx) {
    float dt2 = dt * dt;

    float dydt_local = (6 * dt2 - 6 * dt) * y0 + (3 * dt2 - 4 * dt + 1) * m0 +
                       (-6 * dt2 + 6 * dt) * y1 + (3 * dt2 - 2 * dt) * m1;

    if (segmentDx > 0.0001f) {
        return dydt_local / segmentDx;
    }
    return 0;
}

float SplinePattern::evaluate(float t) const {
    if (_points.empty()) return 0;
    if (_points.size() == 1) return _points[0].y;

    if (_totalDuration > 0) {
        t = fmodf(t, _totalDuration);
        if (t < 0) t += _totalDuration;
    }

    int seg = findSegment(t);
    const SplinePoint& p0 = _points[seg];
    const SplinePoint& p1 = _points[seg + 1];

    float segDx = p1.x - p0.x;
    if (segDx < 0.0001f) return p0.y;

    float dt = (t - p0.x) / segDx;

    float d0 = (fabsf(p0.handleOutX) > 0.0001f)
                   ? (p0.handleOutY / p0.handleOutX)
                   : 0.0f;
    float d1 =
        (fabsf(p1.handleInX) > 0.0001f) ? (p1.handleInY / p1.handleInX) : 0.0f;

    float m0 = d0 * segDx;
    float m1 = d1 * segDx;

    return hermite(dt, p0.y, p1.y, m0, m1);
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

    float segDx = p1.x - p0.x;
    if (segDx < 0.0001f) return 0;

    float dt = (t - p0.x) / segDx;

    float d0 = (fabsf(p0.handleOutX) > 0.0001f)
                   ? (p0.handleOutY / p0.handleOutX)
                   : 0.0f;
    float d1 =
        (fabsf(p1.handleInX) > 0.0001f) ? (p1.handleInY / p1.handleInX) : 0.0f;

    float m0 = d0 * segDx;
    float m1 = d1 * segDx;

    return hermiteDerivative(dt, p0.y, p1.y, m0, m1, segDx);
}
