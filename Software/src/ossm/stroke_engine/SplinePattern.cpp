#include "SplinePattern.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <math.h>
#include <stdio.h>

#include <algorithm>
#include <vector>

static const char* TAG = "SplinePattern";

// ─── Small math helpers ──────────────────────────────────────────────────

namespace {

    // Per-anchor view of one JSON entry. The optional handleIn / handleOut
    // tangent fields are ignored: tinyspline's natural cubic interpolator
    // derives its own C2-continuous tangents from anchor positions alone.
    struct Anchor {
        double t;
        double y;
    };

    struct TY {
        double t;
        double y;
    };

    // Evaluate the spline at a single knot-t value and copy the first 2D point
    // out of the resulting De Boor net. Allocates / frees a tsDeBoorNet per
    // call; both operations are cheap (single small malloc), but keep this off
    // the hot path where possible. Returns true on success.
    bool evalTY(const tsBSpline* spline, tsReal knotT, TY* out) {
        tsDeBoorNet net = ts_deboornet_init();
        tsStatus status;
        bool ok = false;
        if (ts_bspline_eval(spline, knotT, &net, &status) == TS_SUCCESS) {
            const tsReal* result = ts_deboornet_result_ptr(&net);
            if (result != nullptr) {
                out->t = result[0];
                out->y = result[1];
                ok = true;
            }
        }
        ts_deboornet_free(&net);
        return ok;
    }

}  // namespace

// ─── Lifecycle ───────────────────────────────────────────────────────────

SplinePattern::SplinePattern()
    : _spline(ts_bspline_init()),
      _splineDeriv1(ts_bspline_init()),
      _splineDeriv2(ts_bspline_init()) {
    startTime = millis();
    timeDelta = 0;
    currentSplinePercent = 0.0f;
}

SplinePattern::~SplinePattern() { freeSplines(); }

void SplinePattern::freeSplines() {
    ts_bspline_free(&_spline);
    ts_bspline_free(&_splineDeriv1);
    ts_bspline_free(&_splineDeriv2);
    _spline = ts_bspline_init();
    _splineDeriv1 = ts_bspline_init();
    _splineDeriv2 = ts_bspline_init();
    _splineReady = false;
}

// ─── Loading ─────────────────────────────────────────────────────────────

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

    bool ok = loadFromJson(id, buf, playRangeMm, maxSpeedMmPerSec);
    free(buf);
    return ok;
}

bool SplinePattern::loadFromJson(const char* id, const char* jsonText,
                                 float playRangeMm, float maxSpeedMmPerSec) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonText);

    if (error) {
        ESP_LOGE(TAG, "JSON parse failed for %s: %s", id, error.c_str());
        return false;
    }

    const char* patternName = doc["name"] | "Spline";
    strncpy(_name, patternName, sizeof(_name) - 1);
    _name[sizeof(_name) - 1] = '\0';

    JsonArray spline = doc["cubicSpline"];
    if (spline.isNull() || spline.size() < 2) {
        ESP_LOGE(TAG, "cubicSpline missing or too short in %s", id);
        return false;
    }

    std::vector<Anchor> anchors;
    anchors.reserve(spline.size());
    for (JsonObject pt : spline) {
        Anchor a{};
        a.t = pt["t"] | pt["x"] | 0.0;
        a.y = pt["y"] | 0.0;
        anchors.push_back(a);
    }

    // Drop the closing point at t≈1 if present. We treat the pattern as
    // periodic with period 1, so the value at t=1 is identical to t=0 and
    // would be duplicated by the shifted copy of the first anchor below.
    if (!anchors.empty() && anchors.back().t >= 0.9999) {
        anchors.pop_back();
    }

    if (anchors.size() < 2) {
        ESP_LOGE(TAG, "Not enough anchors to build a periodic spline");
        freeSplines();
        return false;
    }

    for (size_t i = 1; i < anchors.size(); i++) {
        if (anchors[i].t <= anchors[i - 1].t) {
            ESP_LOGE(TAG,
                     "cubicSpline t values must be strictly increasing "
                     "(index %u: %f <= %f)",
                     (unsigned)i, anchors[i].t, anchors[i - 1].t);
            freeSplines();
            return false;
        }
    }

    _baseAnchorCount = anchors.size();

    // Tile anchors at offsets -1, 0, +1 so the wrap-around segment from the
    // last anchor back to the first stays smooth. We always evaluate against
    // the middle tile, so the tile-boundary endpoints are far from where we
    // sample.
    std::vector<Anchor> tiled;
    tiled.reserve(anchors.size() * 3 + 1);
    for (int k = -1; k <= 1; ++k) {
        for (const Anchor& a : anchors) {
            Anchor shifted = a;
            shifted.t = a.t + k;
            tiled.push_back(shifted);
        }
    }
    // Close the tiled chain with a copy of the first anchor offset by +2 so
    // the wrap-around segment from the last anchor of tile +1 has a target.
    Anchor closing = anchors.front();
    closing.t = anchors.front().t + 2.0;
    tiled.push_back(closing);

    // Hand the tiled (t, y) anchors to tinyspline's natural-cubic interpolator.
    // It returns a sequence of cubic Beziers (TS_BEZIERS, 4 control points per
    // span) with C2-continuous tangents derived from the anchor geometry — the
    // same shape the previous manual loop produced when handleIn == handleOut
    // and matched the segment dx, but without any per-pattern hand-tuning.
    std::vector<tsReal> pts;
    pts.reserve(tiled.size() * 2);
    for (const Anchor& a : tiled) {
        pts.push_back((tsReal)a.t);
        pts.push_back((tsReal)a.y);
    }

    // (Re)build the tinyspline objects. freeSplines() resets them to a known
    // initialised state so the destructor is safe even if we bail mid-way.
    freeSplines();

    tsStatus status;
    if (ts_bspline_interpolate_cubic_natural(pts.data(), tiled.size(), 2,
                                             &_spline, &status) != TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_interpolate_cubic_natural failed for %s: %s",
                 id, status.message);
        freeSplines();
        return false;
    }
    // Pass epsilon < 0 to skip tinyspline's strict internal-knot continuity
    // check; the interpolator guarantees continuity by construction but
    // accumulates 1e-7-ish float residuals in the knot vector.
    if (ts_bspline_derive(&_spline, 1, -1.0, &_splineDeriv1, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_derive(1) failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    if (ts_bspline_derive(&_spline, 2, -1.0, &_splineDeriv2, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_derive(2) failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    _splineReady = true;

    const size_t segmentCount = ts_bspline_num_control_points(&_spline) / 4;
    const double dtPerSeg =
        (segmentCount > 0) ? 1.0 / (double)segmentCount : 0.0;

    // Knot range for one period: the middle tile only. The full spline knot
    // domain [0, 1] spans three tiled copies; mapping period t ∈ [0, 1] to the
    // entire knot range would traverse three periods instead of one.
    const size_t midStart = _baseAnchorCount;
    const size_t midEnd = 2 * _baseAnchorCount;
    _knotTMin = (segmentCount > 0) ? (double)midStart * dtPerSeg : 0.0;
    _knotTMax = (segmentCount > 0) ? (double)midEnd * dtPerSeg : 1.0;

    // Approximate the steepest slope by sampling each base-period segment.
    // The per-evaluate cost of this is paid once at load. We need the segment
    // that yields the largest |dy/dt| across normalized t ∈ [0, 1] so we can
    // compute totalDuration consistent with the motor's mm/s ceiling.
    constexpr int kSlopeSamples = 128;
    float maxSlopeAbs = 0.0f;
    float dtAtMaxSlopePercent = 0.0f;

    for (size_t segIdx = midStart; segIdx < midEnd && segIdx < segmentCount;
         ++segIdx) {
        const double knotTStart = (double)segIdx * dtPerSeg;
        const double knotTEnd = (double)(segIdx + 1) * dtPerSeg;
        const double segDt = tiled[segIdx + 1].t - tiled[segIdx].t;

        for (int s = 0; s <= kSlopeSamples; ++s) {
            const double frac = (double)s / kSlopeSamples;
            const tsReal knotT =
                (tsReal)(knotTStart + frac * (knotTEnd - knotTStart));
            TY d1{};
            if (!evalTY(&_splineDeriv1, knotT, &d1)) continue;
            if (fabs(d1.t) < 1e-9) continue;
            const float slope = (float)fabs(d1.y / d1.t);
            if (slope > maxSlopeAbs) {
                maxSlopeAbs = slope;
                dtAtMaxSlopePercent = (float)segDt;
            }
        }
    }

    const float dtAtMaxSlopeSeconds = playRangeMm / maxSpeedMmPerSec;

    // dtAtMaxSlopePercent is the t-span (in normalized period units) of the
    // segment containing the steepest sample. Treating that segment as the
    // bottleneck, the wall-clock time to traverse it equals
    // dtAtMaxSlopeSeconds, so the full period scales as 1 / dtPercent.
    _totalDuration = (dtAtMaxSlopePercent > 0.0f)
                         ? dtAtMaxSlopeSeconds / dtAtMaxSlopePercent
                         : 0.0f;

    ESP_LOGI(TAG,
             "Loaded '%s': %u anchors (tiled to %u segments), maxSlope=%.3f, "
             "duration=%.3fs",
             _name, (unsigned)_baseAnchorCount, (unsigned)segmentCount,
             maxSlopeAbs, _totalDuration);

    return true;
}

// ─── Evaluation ──────────────────────────────────────────────────────────

SplineSample SplinePattern::sampleAtPeriod(double speedPercent, float period,
                                           double& lastSpeed, double& tOffset) {
    if (!_splineReady || _baseAnchorCount < 2 || period <= 0.0f ||
        speedPercent <= 0.0) {
        return SplineSample{0.0, 0.0, 0.0, 0.0, speedPercent, 0.0};
    }

    // ── Time → normalized t in [0, 1] ────────────────────────────────────
    // Identical to the original implementation. Speed changes mid-stroke
    // adjust `tOffset` so the curve doesn't jump when the period scales.
    long deltaTimeMS = millis() - startTime;

    long timeSinceStartMS = fmod(deltaTimeMS, period * 1000 / speedPercent);

    if (lastSpeed > 0 && lastSpeed != speedPercent) {
        long lastTimeSinceStartMS =
            fmod(deltaTimeMS, period * 1000 / lastSpeed);
        tOffset += timeSinceStartMS / (period * 1000 / speedPercent) -
                   lastTimeSinceStartMS / (period * 1000 / lastSpeed);
    }

    lastSpeed = speedPercent;

    double t = timeSinceStartMS / (period * 1000 / speedPercent) - tOffset;
    if (t < 0.0) {
        t = 1.0 + fmod(t, 1.0);
    } else if (t > 1.0) {
        t = fmod(t, 1.0);
    }

    const double knotT = _knotTMin + t * (_knotTMax - _knotTMin);

    TY pos{};
    TY vel{};
    TY acc{};
    evalTY(&_spline, (tsReal)knotT, &pos);
    evalTY(&_splineDeriv1, (tsReal)knotT, &vel);
    evalTY(&_splineDeriv2, (tsReal)knotT, &acc);

    // Pick the "y" of the anchor at the end of the current segment. With
    // uniform TS_BEZIERS knots each segment is 4 control points wide and the
    // last (P3) is the segment's end anchor; this matches the prior
    // evalTY(knotTEnd) result by C0 continuity but skips the De Boor allocation.
    const size_t numSeg = ts_bspline_num_control_points(&_spline) / 4;
    double endY = 0.0;
    if (numSeg > 0) {
        size_t segIdx = static_cast<size_t>(knotT * numSeg);
        if (segIdx >= numSeg) segIdx = numSeg - 1;
        const tsReal* cp = nullptr;
        tsStatus status;
        if (ts_bspline_control_point_at_ptr(&_spline, segIdx * 4 + 3, &cp,
                                            &status) == TS_SUCCESS &&
            cp != nullptr) {
            endY = cp[1];
        }
    }

    return SplineSample{
        t, pos.y, vel.y, acc.y, speedPercent, endY,
    };
}

SplineSample SplinePattern::evaluate(double speedPercent) {
    return sampleAtPeriod(speedPercent, _totalDuration, lastSpeedPercent,
                          timeOffset);
}
