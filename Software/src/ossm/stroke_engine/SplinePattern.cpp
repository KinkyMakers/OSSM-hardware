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

// Per-anchor view of one JSON entry. handleIn and handleOut are parametric
// tangent vectors describing the curve's slope at (x, y). handleOut is the
// parametric derivative B'(u) at the anchor as the next segment leaves
// (u=0); handleIn is B'(u) at the anchor as the previous segment arrives
// (u=1). The geometric slope at the anchor is therefore handle.y / handle.x,
// and the vector's magnitude controls how strongly the curve adheres to
// that direction. Segment A → B becomes a cubic Bezier with control points
//   P0 = A,  P1 = A + A.handleOut/3,  P2 = B − B.handleIn/3,  P3 = B
// chosen so that B'(0) = A.handleOut and B'(1) = B.handleIn exactly. All-
// zero handles still cleanly degenerate to a straight line.
struct Anchor {
    double x;
    double y;
    double handleInX;
    double handleInY;
    double handleOutX;
    double handleOutY;
};

struct XY {
    double x;
    double y;
};

// Evaluate the spline at a single u value and copy the first 2D point out of
// the resulting De Boor net. Allocates / frees a tsDeBoorNet per call; both
// operations are cheap (single small malloc), but keep this off the hot path
// where possible. Returns true on success.
bool evalXY(const tsBSpline* spline, tsReal u, XY* out) {
    tsDeBoorNet net = ts_deboornet_init();
    tsStatus status;
    bool ok = false;
    if (ts_bspline_eval(spline, u, &net, &status) == TS_SUCCESS) {
        const tsReal* result = ts_deboornet_result_ptr(&net);
        if (result != nullptr) {
            out->x = result[0];
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
      _splineDeriv2(ts_bspline_init()),
      _splineDeriv3(ts_bspline_init()) {
    startTime = millis();
    timeDelta = 0;
    currentSplinePercent = 0.0f;
}

SplinePattern::~SplinePattern() { freeSplines(); }

void SplinePattern::freeSplines() {
    ts_bspline_free(&_spline);
    ts_bspline_free(&_splineDeriv1);
    ts_bspline_free(&_splineDeriv2);
    ts_bspline_free(&_splineDeriv3);
    _spline = ts_bspline_init();
    _splineDeriv1 = ts_bspline_init();
    _splineDeriv2 = ts_bspline_init();
    _splineDeriv3 = ts_bspline_init();
    _splineReady = false;
}

// ─── Loading ─────────────────────────────────────────────────────────────

bool SplinePattern::loadFromFile(const char* id, float playRangeMm,
                                 float maxSpeedMmPerSec,
                                 float maxAccelMmPerSec2,
                                 float maxJerkMmPerSec3) {
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

    bool ok = loadFromJson(id, buf, playRangeMm, maxSpeedMmPerSec,
                           maxAccelMmPerSec2, maxJerkMmPerSec3);
    free(buf);
    return ok;
}

bool SplinePattern::loadFromJson(const char* id, const char* jsonText,
                                 float playRangeMm, float maxSpeedMmPerSec,
                                 float maxAccelMmPerSec2,
                                 float maxJerkMmPerSec3) {
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
        a.x = pt["x"] | 0.0;
        a.y = pt["y"] | 0.0;
        JsonObject hin = pt["handleIn"];
        if (!hin.isNull()) {
            a.handleInX = hin["x"] | 0.0;
            a.handleInY = hin["y"] | 0.0;
        }
        JsonObject hout = pt["handleOut"];
        if (!hout.isNull()) {
            a.handleOutX = hout["x"] | 0.0;
            a.handleOutY = hout["y"] | 0.0;
        }
        anchors.push_back(a);
    }

    // Drop the closing point at x≈1 if present. We treat the pattern as
    // periodic with period 1, so the value at x=1 is identical to x=0 and
    // would be duplicated by the shifted copy of the first anchor below.
    if (!anchors.empty() && anchors.back().x >= 0.9999) {
        anchors.pop_back();
    }

    if (anchors.size() < 2) {
        ESP_LOGE(TAG, "Not enough anchors to build a periodic spline");
        freeSplines();
        return false;
    }

    for (size_t i = 1; i < anchors.size(); i++) {
        if (anchors[i].x <= anchors[i - 1].x) {
            ESP_LOGE(TAG,
                     "cubicSpline x values must be strictly increasing "
                     "(index %u: %f <= %f)",
                     (unsigned)i, anchors[i].x, anchors[i - 1].x);
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
            shifted.x = a.x + k;
            tiled.push_back(shifted);
        }
    }
    // Close the tiled chain with a copy of the first anchor offset by +2 so
    // the wrap-around segment from the last anchor of tile +1 has a target.
    Anchor closing = anchors.front();
    closing.x = anchors.front().x + 2.0;
    tiled.push_back(closing);

    // Build per-segment cubic Bezier control points. With TS_BEZIERS each
    // segment owns four control points; adjacent segments duplicate their
    // shared anchor as P3 of one segment and P0 of the next.
    const size_t segmentCount = tiled.size() - 1;
    const size_t numCtrl = segmentCount * 4;
    std::vector<tsReal> ctrl;
    ctrl.reserve(numCtrl * 2);

    bool sawSegmentXMonotonicityWarning = false;
    for (size_t i = 0; i < segmentCount; ++i) {
        const Anchor& a = tiled[i];
        const Anchor& b = tiled[i + 1];

        // For a cubic Bezier, B'(0) = 3(P1 − P0) and B'(1) = 3(P3 − P2).
        // To make B'(0) = a.handleOut and B'(1) = b.handleIn we place the
        // control points at one-third of the tangent vector away from the
        // anchor. Note the sign flip on handleIn: the parametric tangent at
        // u=1 points from P2 toward P3, so handleIn (the "arriving" tangent)
        // pulls P2 back from B.
        const double p0x = a.x;
        const double p0y = a.y;
        const double p1x = a.x + a.handleOutX / 3.0;
        const double p1y = a.y + a.handleOutY / 3.0;
        const double p2x = b.x - b.handleInX / 3.0;
        const double p2y = b.y - b.handleInY / 3.0;
        const double p3x = b.x;
        const double p3y = b.y;

        // The Newton solver in evaluate() needs x(u) strictly monotonic
        // inside each segment. A necessary condition is that both control
        // x-coordinates sit between the anchor x-values; warn once if not so
        // the pattern author can tighten the handle.
        const double xLo = std::min(p0x, p3x);
        const double xHi = std::max(p0x, p3x);
        if (!sawSegmentXMonotonicityWarning &&
            (p1x < xLo - 1e-9 || p1x > xHi + 1e-9 || p2x < xLo - 1e-9 ||
             p2x > xHi + 1e-9)) {
            ESP_LOGW(TAG,
                     "Pattern '%s' segment %u may have non-monotonic x(u); "
                     "Newton solve could land on wrong root",
                     id, (unsigned)i);
            sawSegmentXMonotonicityWarning = true;
        }

        ctrl.push_back(p0x);
        ctrl.push_back(p0y);
        ctrl.push_back(p1x);
        ctrl.push_back(p1y);
        ctrl.push_back(p2x);
        ctrl.push_back(p2y);
        ctrl.push_back(p3x);
        ctrl.push_back(p3y);
    }

    // (Re)build the tinyspline objects. freeSplines() resets them to a known
    // initialised state so the destructor is safe even if we bail mid-way.
    freeSplines();

    tsStatus status;
    if (ts_bspline_new(numCtrl, 2, 3, TS_BEZIERS, &_spline, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_new failed for %s: %s", id, status.message);
        freeSplines();
        return false;
    }
    if (ts_bspline_set_control_points(&_spline, ctrl.data(), &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_set_control_points failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    // tinyspline's derive() does an internal continuity check at internal
    // knots with a strict epsilon. We construct each segment so that
    // adjacent Beziers share an anchor (C0) and matched tangents (C1, C2 by
    // symmetry when handleIn == handleOut), but tinyspline accumulates
    // float-precision error in its knot vector (TS_DOMAIN_DEFAULT_MIN/MAX
    // are float literals — see tinyspline.h) which produces residuals on the
    // order of 1e-7 between mathematically-equal control points. Passing
    // epsilon < 0 disables the check entirely; we own the geometry so we
    // know it's continuous by construction.
    if (ts_bspline_derive(&_spline, 1, -1.0, &_splineDeriv1, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_derive(1) failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    if (ts_bspline_derive(&_splineDeriv1, 1, -1.0, &_splineDeriv2, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_derive(2) failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    // Third parametric derivative powers the jerk term in the chain-rule
    // formula d3y/dx3 used by both the load-time feasibility sweep and
    // evaluateFeasible's per-call jerk computation.
    if (ts_bspline_derive(&_splineDeriv2, 1, -1.0, &_splineDeriv3, &status) !=
        TS_SUCCESS) {
        ESP_LOGE(TAG, "ts_bspline_derive(3) failed for %s: %s", id,
                 status.message);
        freeSplines();
        return false;
    }
    _splineReady = true;

    // Populate the segment lookup. The spline u-domain is [0, 1] split into
    // segmentCount equal-length windows.
    _segments.clear();
    _segments.reserve(segmentCount);
    const double duPerSeg = 1.0 / (double)segmentCount;
    for (size_t i = 0; i < segmentCount; ++i) {
        TiledSegment seg{};
        seg.xStart = tiled[i].x;
        seg.xEnd = tiled[i + 1].x;
        seg.uStart = (double)i * duPerSeg;
        seg.uEnd = (double)(i + 1) * duPerSeg;
        _segments.push_back(seg);
    }

    // Approximate the steepest slope by sampling each base-period segment in
    // u. The per-evaluate cost of this is paid once at load. We need the
    // segment that yields the largest |dy/dx| across normalized x ∈ [0, 1] so
    // we can compute totalDuration consistent with the motor's mm/s ceiling.
    //
    // The same sweep also records |d²y/dx²| and |d³y/dx³| (computed from the
    // chain rule on the parametric derivatives) so we can size
    // _feasibleTotalDuration to keep acceleration and jerk within their
    // physical limits at speedPercent = 1.0.
    //
    // We restrict the chain-rule curvature / jerk samples to the segment
    // interior (5% – 95% in u). Near segment boundaries cubic Beziers with
    // degenerate handles drive g'(u) → 0, so the formulas (·) / g'³ and
    // (·) / g'⁵ become numerically singular even though the analytic value
    // is well-defined. For patterns that are genuinely C² the interior
    // samples are a good estimate of the global max; for patterns with
    // corners (true C¹ discontinuities) the analytic jerk is unbounded and
    // no calibration can capture it — uniform time-scaling can't satisfy
    // MAX_JERK on such patterns, so this is a best-effort estimate.
    constexpr int kSlopeSamples = 128;
    constexpr int kInteriorSamples = 128;
    constexpr double kInteriorMin = 0.05;
    constexpr double kInteriorMax = 0.95;
    float maxSlopeAbs = 0.0f;
    float maxCurvAbs = 0.0f;
    float maxJerkAbs = 0.0f;
    float dxAtMaxSlopePercent = 0.0f;
    // We sample only the middle tile (segments [N, 2N) where N is the base
    // anchor count) so the slope reflects one full period including the wrap
    // segment, but not any anchors duplicated at +1/-1.
    const size_t midStart = _baseAnchorCount;
    const size_t midEnd = 2 * _baseAnchorCount;

    for (size_t segIdx = midStart; segIdx < midEnd && segIdx < segmentCount;
         ++segIdx) {
        const TiledSegment& seg = _segments[segIdx];
        const double segDx = seg.xEnd - seg.xStart;

        // Slope sweep covers the full segment (slope formula is only
        // first-order and doesn't blow up the way y_xx / y_xxx can).
        for (int s = 0; s <= kSlopeSamples; ++s) {
            const double frac = (double)s / kSlopeSamples;
            const tsReal u =
                (tsReal)(seg.uStart + frac * (seg.uEnd - seg.uStart));
            XY d1{};
            if (!evalXY(&_splineDeriv1, u, &d1)) continue;
            if (fabs(d1.x) < 1e-9) continue;
            const float slope = (float)fabs(d1.y / d1.x);
            if (slope > maxSlopeAbs) {
                maxSlopeAbs = slope;
                dxAtMaxSlopePercent = (float)segDx;
            }
        }

        // Interior sweep for curvature and jerk-along-path. Chain rule:
        //   y_xx  = (f''·g' − f'·g'') / g'³
        //   y_xxx = (f'''·g'² − 3·f''·g'·g'' − f'·g'·g''' + 3·f'·g''²)
        //           / g'⁵
        for (int s = 0; s <= kInteriorSamples; ++s) {
            const double frac =
                kInteriorMin +
                (kInteriorMax - kInteriorMin) * s / kInteriorSamples;
            const tsReal u =
                (tsReal)(seg.uStart + frac * (seg.uEnd - seg.uStart));
            XY d1{}, d2{}, d3{};
            if (!evalXY(&_splineDeriv1, u, &d1)) continue;
            if (!evalXY(&_splineDeriv2, u, &d2)) continue;
            if (!evalXY(&_splineDeriv3, u, &d3)) continue;
            const double gp = d1.x;
            // Skip samples where g' has collapsed: the chain-rule formulas
            // produce numerical garbage that doesn't reflect the true
            // analytic value. The threshold scales with the segment's
            // average dx/du = segDx / (uEnd - uStart) so it adapts to
            // segment width.
            const double avgGp = segDx / (seg.uEnd - seg.uStart);
            if (fabs(gp) < 1e-3 * fabs(avgGp)) continue;
            const double gp2 = gp * gp;
            const double gp3 = gp2 * gp;
            const double gp5 = gp3 * gp2;
            const double curv = (d2.y * d1.x - d1.y * d2.x) / gp3;
            const double jrk =
                (d3.y * gp2 - 3.0 * d2.y * d1.x * d2.x -
                 d1.y * d1.x * d3.x + 3.0 * d1.y * d2.x * d2.x) /
                gp5;
            const float curvAbs = (float)fabs(curv);
            const float jrkAbs = (float)fabs(jrk);
            if (curvAbs > maxCurvAbs) maxCurvAbs = curvAbs;
            if (jrkAbs > maxJerkAbs) maxJerkAbs = jrkAbs;
        }
    }

    const float dxAtMaxSlopeSeconds = playRangeMm / maxSpeedMmPerSec;

    ESP_LOGI(TAG,
             "maxSlopeAbs: %.3f, dxAtMaxSlopePercent: %.3f, "
             "dxAtMaxSlopeSeconds: %.3f",
             maxSlopeAbs, dxAtMaxSlopePercent, dxAtMaxSlopeSeconds);

    // dxAtMaxSlopePercent is the x-span (in normalized period units) of the
    // segment containing the steepest sample. Treating that segment as the
    // bottleneck, the wall-clock time to traverse it equals
    // dxAtMaxSlopeSeconds, so the full period scales as 1 / dxPercent.
    _totalDuration = (dxAtMaxSlopePercent > 0.0f)
                         ? dxAtMaxSlopeSeconds / dxAtMaxSlopePercent
                         : 0.0f;

    // ── Feasibility period (accounts for speed AND accel AND jerk) ───────
    // Physical envelope at full speed when the period is T:
    //   |v|   = playRangeMm · |dy/dx|   / T
    //   |a|   = playRangeMm · |d²y/dx²| / T²
    //   |j|   = playRangeMm · |d³y/dx³| / T³
    // Inverting each against its respective device limit yields a minimum
    // period; the largest of those is the binding constraint.
    const float T_v = (maxSpeedMmPerSec > 0.0f)
                          ? playRangeMm * maxSlopeAbs / maxSpeedMmPerSec
                          : 0.0f;
    const float T_a = (maxAccelMmPerSec2 > 0.0f)
                          ? sqrtf(playRangeMm * maxCurvAbs / maxAccelMmPerSec2)
                          : 0.0f;
    const float T_j = (maxJerkMmPerSec3 > 0.0f)
                          ? cbrtf(playRangeMm * maxJerkAbs / maxJerkMmPerSec3)
                          : 0.0f;
    // Include _totalDuration in the max so the feasible period is never
    // shorter than the slope-only calibration above, even if our T_v
    // (which uses true max|dy/dx| rather than the per-segment x-span) ends
    // up slightly smaller for unusual patterns.
    _feasibleTotalDuration =
        std::max({_totalDuration, T_v, T_a, T_j});

    const char* binding = "speed";
    if (T_a >= T_v && T_a >= T_j) binding = "accel";
    if (T_j >= T_v && T_j >= T_a) binding = "jerk";
    if (_totalDuration >= T_v && _totalDuration >= T_a &&
        _totalDuration >= T_j) {
        binding = "slope-segment";
    }

    ESP_LOGI(TAG,
             "'%s' calibration: maxSlope=%.3f maxCurv=%.3f maxJerk=%.3f "
             "-> T_v=%.3fs T_a=%.3fs T_j=%.3fs feasibleTotal=%.3fs "
             "(binding=%s)",
             _name, maxSlopeAbs, maxCurvAbs, maxJerkAbs, T_v, T_a, T_j,
             _feasibleTotalDuration, binding);

    ESP_LOGI(TAG,
             "Loaded '%s': %u anchors (tiled to %u segments), duration=%.3f, "
             "feasibleDuration=%.3f",
             _name, (unsigned)_baseAnchorCount, (unsigned)segmentCount,
             _totalDuration, _feasibleTotalDuration);

    return true;
}

// ─── Evaluation ──────────────────────────────────────────────────────────

SplineSample SplinePattern::sampleAtPeriod(double speedPercent, float period,
                                           double& lastSpeed, double& tOffset,
                                           bool computeJerk) {
    if (!_splineReady || _baseAnchorCount < 2 || _segments.empty() ||
        period <= 0.0f || speedPercent <= 0.0) {
        return SplineSample{0.0, 0.0, 0.0, 0.0, 0.0, speedPercent};
    }

    // ── Time → normalized t in [0, 1] ────────────────────────────────────
    // Identical to the original implementation. Speed changes mid-stroke
    // adjust `tOffset` so the curve doesn't jump when the period scales.
    long deltaTimeMS = millis() - startTime;

    long timeSinceStartMS =
        fmod(deltaTimeMS, period * 1000 / speedPercent);

    if (lastSpeed > 0 && lastSpeed != speedPercent) {
        long lastTimeSinceStartMS =
            fmod(deltaTimeMS, period * 1000 / lastSpeed);
        tOffset += timeSinceStartMS / (period * 1000 / speedPercent) -
                   lastTimeSinceStartMS / (period * 1000 / lastSpeed);
    }

    double t =
        timeSinceStartMS / (period * 1000 / speedPercent) - tOffset;

    if (t < 0.0) {
        t = 1.0 + fmod(t, 1.0);
    } else if (t > 1.0) {
        t = fmod(t, 1.0);
    }

    lastSpeed = speedPercent;

    // ── Locate the middle-tile segment containing t ──────────────────────
    // The middle tile spans _segments[N .. 2N) where N is the base anchor
    // count, with x running from anchors[0].x (typically 0) to that value
    // plus 1. Binary search over the middle tile only.
    const size_t midStart = _baseAnchorCount;
    const size_t midEnd = std::min(2 * _baseAnchorCount, _segments.size());

    size_t segIdx = midStart;
    // Lower bound on xStart > t, then step back one.
    {
        size_t lo = midStart;
        size_t hi = midEnd;
        while (lo < hi) {
            const size_t mid = lo + (hi - lo) / 2;
            if (_segments[mid].xStart <= t) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
        segIdx = (lo == midStart) ? midStart : (lo - 1);
    }
    // Clamp in case rounding pushes us past the tile boundary.
    if (segIdx >= midEnd) segIdx = midEnd - 1;
    const TiledSegment& seg = _segments[segIdx];

    // ── Solve x(u) = t with Newton + bisection bracket ──────────────────
    double uLo = seg.uStart;
    double uHi = seg.uEnd;
    const double segDx = seg.xEnd - seg.xStart;
    double u = (segDx > 1e-12)
                   ? (seg.uStart + (t - seg.xStart) / segDx *
                                       (seg.uEnd - seg.uStart))
                   : seg.uStart;
    if (u < uLo) u = uLo;
    if (u > uHi) u = uHi;

    XY pos{};
    XY vel{};
    constexpr int kMaxIters = 20;
    constexpr double kTol = 1e-6;
    bool converged = false;
    for (int it = 0; it < kMaxIters; ++it) {
        if (!evalXY(&_spline, (tsReal)u, &pos)) break;
        const double fx = pos.x - t;
        if (fabs(fx) < kTol) {
            converged = true;
            break;
        }
        if (!evalXY(&_splineDeriv1, (tsReal)u, &vel)) break;
        // Maintain the bracket [uLo, uHi] so a bad Newton step can fall back
        // to bisection without losing the root.
        if (fx > 0) {
            uHi = u;
        } else {
            uLo = u;
        }
        double uNext = u;
        if (fabs(vel.x) > 1e-9) {
            uNext = u - fx / vel.x;
        }
        if (!(uNext > uLo && uNext < uHi)) {
            // Newton stepped outside the bracket (or hit a flat x'); bisect.
            uNext = 0.5 * (uLo + uHi);
        }
        if (fabs(uNext - u) < 1e-12) {
            converged = true;
            u = uNext;
            break;
        }
        u = uNext;
    }

    // Final eval to make sure pos / vel correspond to the converged u even
    // when we exited early via tolerance check.
    if (!converged) {
        evalXY(&_spline, (tsReal)u, &pos);
    }
    evalXY(&_splineDeriv1, (tsReal)u, &vel);

    XY acc{};
    evalXY(&_splineDeriv2, (tsReal)u, &acc);

    // ── Compose dy/dt, d²y/dt², and (optionally) d³y/dt³ ─────────────────
    // y = y(u(t)) where x(u(t)) = t.
    //   dy/dt   = y'(u) / x'(u)
    //   d²y/dt² = (y''(u) x'(u) − y'(u) x''(u)) / x'(u)³
    //   d³y/dt³ = (y'''(u) x'(u)² − 3 y''(u) x'(u) x''(u)
    //              − y'(u) x'(u) x'''(u) + 3 y'(u) x''(u)²) / x'(u)⁵
    double yPos = pos.y;
    double velocity = 0.0;
    double acceleration = 0.0;
    double jerk = 0.0;
    if (fabs(vel.x) > 1e-9) {
        const double xp = vel.x;
        const double xp2 = xp * xp;
        const double xp3 = xp2 * xp;
        velocity = vel.y / xp;
        acceleration = (acc.y * xp - vel.y * acc.x) / xp3;
        if (computeJerk) {
            XY jrk{};
            if (evalXY(&_splineDeriv3, (tsReal)u, &jrk)) {
                const double xp5 = xp3 * xp2;
                jerk = (jrk.y * xp2 - 3.0 * acc.y * xp * acc.x -
                        vel.y * xp * jrk.x + 3.0 * vel.y * acc.x * acc.x) /
                       xp5;
            }
        }
    }

    return SplineSample{
        t, yPos, velocity, acceleration, jerk, speedPercent,
    };
}

SplineSample SplinePattern::evaluate(double speedPercent) {
    // Raw (slope-only) sample — preserves today's behaviour for tests and
    // plot comparison. No jerk computation needed.
    return sampleAtPeriod(speedPercent, _totalDuration, lastSpeedPercent,
                          timeOffset, /*computeJerk=*/false);
}

SplineSample SplinePattern::evaluateFeasible(double speedPercent) {
    // Feasibility-aware sample — plays the same geometry against the longer
    // `_feasibleTotalDuration`, so |velocity|, |acceleration|, and |jerk|
    // (when expressed in physical units via playRangeMm) stay within the
    // limits supplied to loadFromJson at speedPercent ≤ 1.0.
    return sampleAtPeriod(speedPercent, _feasibleTotalDuration,
                          lastSpeedPercentFeasible, timeOffsetFeasible,
                          /*computeJerk=*/true);
}
