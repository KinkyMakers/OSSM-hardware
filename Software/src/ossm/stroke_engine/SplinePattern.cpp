#include "SplinePattern.h"

#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>
#include <stdio.h>

#include <vector>

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

    std::vector<double> xs;
    std::vector<double> ys;
    xs.reserve(spline.size());
    ys.reserve(spline.size());

    for (JsonObject pt : spline) {
        xs.push_back(pt["x"] | 0.0);
        ys.push_back(pt["y"] | 0.0);
    }

    // Drop the closing point at x=1 if present. We treat the pattern as
    // periodic with period 1, so the value at x=1 is identical to x=0 and
    // would be duplicated by the shifted copy of the first point below.
    if (!xs.empty() && xs.back() >= 0.9999) {
        xs.pop_back();
        ys.pop_back();
    }

    if (xs.size() < 2) {
        ESP_LOGE(TAG, "Not enough points to build a periodic spline");
        return false;
    }

    for (size_t i = 1; i < xs.size(); i++) {
        if (xs[i] <= xs[i - 1]) {
            ESP_LOGE(TAG,
                     "cubicSpline x values must be strictly increasing "
                     "(index %u: %f <= %f)",
                     (unsigned)i, xs[i], xs[i - 1]);
            return false;
        }
    }

    // Tile the base period three times at offsets -1, 0, +1 so tk::spline
    // sees a smooth periodic signal across the [0, 1] evaluation window.
    // The spline grid now spans roughly [-1, 2], so the natural-boundary
    // endpoints are far from where we actually sample.
    std::vector<double> xsTiled;
    std::vector<double> ysTiled;
    xsTiled.reserve(xs.size() * 3);
    ysTiled.reserve(ys.size() * 3);
    for (int k = -1; k <= 1; ++k) {
        for (size_t i = 0; i < xs.size(); ++i) {
            xsTiled.push_back(xs[i] + k);
            ysTiled.push_back(ys[i]);
        }
    }

    // Find the steepest segment in normalized (x in [0,1]) space. The
    // motor's peak mm/s must be achievable on that segment, which gives us
    // the minimum real-time period required for a full [0,1] traversal.
    // Include the wrap segment (last point -> first point of next period)
    // so a fast return stroke at the boundary is accounted for.
    // TODO this will not determine the max slope. must use the second
    // derivative = 0
    float maxSlopeAbs = 0.0f;
    float dxAtMaxSlopePercent = 0.0f;
    auto considerSegment = [&](double x0, double y0, double x1, double y1) {
        float dx = (float)(x1 - x0);
        if (dx < 0.0001f) return;
        float slope = fabsf((float)(y1 - y0) / dx);
        if (slope > maxSlopeAbs) {
            maxSlopeAbs = slope;
            dxAtMaxSlopePercent = dx;
        }
    };
    for (size_t i = 1; i < xs.size(); i++) {
        considerSegment(xs[i - 1], ys[i - 1], xs[i], ys[i]);
    }
    considerSegment(xs.back(), ys.back(), xs.front() + 1.0, ys.front());

    float dxAtMaxSlopeSeconds = playRangeMm / maxSpeedMmPerSec;

    ESP_LOGI(TAG,
             "maxSlopeAbs: %.3f, dxAtMaxSlopePercent: %.3f, "
             "dxAtMaxSlopeSeconds: %.3f",
             maxSlopeAbs, dxAtMaxSlopePercent, dxAtMaxSlopeSeconds);

    _totalDuration = (dxAtMaxSlopePercent > 0.0f)
                         ? dxAtMaxSlopeSeconds / dxAtMaxSlopePercent
                         : 0.0f;

    _spline.set_boundary(tk::spline::first_deriv, 1.0f, tk::spline::first_deriv,
                         1.0f);
    _spline.set_boundary(tk::spline::second_deriv, 0.0,
                         tk::spline::second_deriv, 0.0);
    _spline.set_points(xsTiled, ysTiled, tk::spline::cspline_hermite);
    _pointCount = xs.size();

    ESP_LOGI(TAG, "Loaded '%s': %u points (tiled to %u), duration=%.3f", _name,
             (unsigned)_pointCount, (unsigned)xsTiled.size(), _totalDuration);

    return true;
}

SplineSample SplinePattern::evaluate(double t) const {
    if (_pointCount < 2) {
        return SplineSample{0.0, 0.0, 0.0};
    }

    // x grid is normalized to [0, 1]; wrap unbounded caller time into that.
    // The spline itself spans [-1, 2] via periodic tiling, so t=0 and t=1
    // sit well inside the interior and give C2-continuous derivatives.
    t = fmod(t, 1.0);
    if (t < 0.0) t += 1.0;

    return SplineSample{
        (double)_spline(t),
        (double)_spline.deriv(1, t),
        (double)_spline.deriv(2, t),
    };
}
