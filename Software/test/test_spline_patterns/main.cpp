// ┌──────────────────────────────────────────────────────────────────────────┐
// │ SPLINE PATTERN EVALUATION TESTS                                         │
// │                                                                         │
// │ Loads every JSON pattern in Software/data/patterns/ via                 │
// │ SplinePattern::loadFromJson, then drives evaluate(1.0) across 3 full    │
// │ totalDuration cycles using a faked millis() clock. Per-pattern samples  │
// │ are written to Analysis/spline_samples_<id>.csv so plot_spline.py can   │
// │ render position / velocity / acceleration over time.                    │
// └──────────────────────────────────────────────────────────────────────────┘

#include <ArduinoFake.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unity.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "ossm/stroke_engine/SplinePattern.h"

using namespace fakeit;

// ─── Fake clock ──────────────────────────────────────────────────────────

static unsigned long g_fakedMs = 0;

static void resetFakeClock() {
    g_fakedMs = 0;
    ArduinoFakeReset();
    When(Method(ArduinoFake(), millis)).AlwaysDo([]() -> unsigned long {
        return g_fakedMs;
    });
}

// ─── Filesystem helpers ──────────────────────────────────────────────────

// Candidate locations for data/patterns/ and Analysis/ relative to where
// PlatformIO happens to launch the test binary.
static const char* PATTERN_DIR_CANDIDATES[] = {
    "data/patterns",
    "../data/patterns",
    "../../data/patterns",
    "Software/data/patterns",
    "../Software/data/patterns",
};

static const char* ANALYSIS_DIR_CANDIDATES[] = {
    "../Analysis",
    "../../Analysis",
    "Analysis",
    "./Analysis",
};

static bool dirExists(const char* path) {
    struct stat s{};
    return stat(path, &s) == 0 && S_ISDIR(s.st_mode);
}

static std::string resolvePatternDir() {
    for (const char* c : PATTERN_DIR_CANDIDATES) {
        if (dirExists(c)) return c;
    }
    return "";
}

static std::string resolveAnalysisDir() {
    for (const char* c : ANALYSIS_DIR_CANDIDATES) {
        if (dirExists(c)) return c;
    }
    // Fall back to creating Analysis/ next to cwd if nothing matches.
    mkdir("Analysis", 0755);
    return "Analysis";
}

static std::vector<std::string> listJsonIds(const std::string& dir) {
    std::vector<std::string> ids;
    DIR* d = opendir(dir.c_str());
    if (!d) return ids;
    struct dirent* e;
    while ((e = readdir(d)) != nullptr) {
        std::string name = e->d_name;
        const std::string ext = ".json";
        if (name.size() <= ext.size()) continue;
        if (name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
            continue;
        ids.push_back(name.substr(0, name.size() - ext.size()));
    }
    closedir(d);
    std::sort(ids.begin(), ids.end());
    return ids;
}

static std::string slurp(const std::string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return {};
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::string out;
    out.resize(n);
    fread(out.data(), 1, n, f);
    fclose(f);
    return out;
}

// ─── Core sweep: sample evaluate() across 3 full periods ─────────────────

// Test-time physical limits. Picked to mirror Config::Driver while remaining
// independent of the firmware headers so this test stays standalone.
static constexpr float kTestPlayRangeMm = 100.0f;
static constexpr float kTestMaxSpeedMmPerSec = 350.0f;
static constexpr float kTestMaxAccelMmPerSec2 = 50000.0f;
static constexpr float kTestMaxJerkMmPerSec3 = 500000.0f;

struct SweepResult {
    SplineSample t0;
    SplineSample tDuration;  // one full period later
    double maxPos = -1e9;
    double minPos = 1e9;
    // Worst-case physical magnitudes observed along the feasible sweep, in
    // device units (mm, mm/s, mm/s², mm/s³). Used by the bounds assertion.
    double maxFeasibleVelMm = 0.0;
    double maxFeasibleAccMm = 0.0;
    double maxFeasibleJerkMm = 0.0;
};

static SweepResult sweepPattern(const std::string& id, const std::string& json,
                                const std::string& analysisDir) {
    SweepResult result;

    // Constructor calls millis(), so reset clock first.
    resetFakeClock();
    SplinePattern pattern;

    // playRangeMm / maxSpeedMmPerSec only affect totalDuration scaling;
    // they don't change the shape we plot. 100 mm / 350 mm/s ≈ realistic OSSM.
    const bool ok = pattern.loadFromJson(
        id.c_str(), json.c_str(), kTestPlayRangeMm, kTestMaxSpeedMmPerSec,
        kTestMaxAccelMmPerSec2, kTestMaxJerkMmPerSec3);
    TEST_ASSERT_TRUE_MESSAGE(ok, ("loadFromJson failed for " + id).c_str());
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, pattern.totalDuration());
    TEST_ASSERT_GREATER_THAN_FLOAT(0.0f, pattern.feasibleTotalDuration());
    TEST_ASSERT_GREATER_OR_EQUAL(2u, pattern.pointCount());
    // The feasible period can never be shorter than the slope-only one —
    // it just adds extra constraints to the calibration.
    TEST_ASSERT_GREATER_OR_EQUAL_FLOAT(pattern.totalDuration(),
                                       pattern.feasibleTotalDuration());

    const double durationMs =
        static_cast<double>(pattern.totalDuration()) * 1000.0;
    const double feasibleDurationMs =
        static_cast<double>(pattern.feasibleTotalDuration()) * 1000.0;
    // Use the longer of the two periods to size the sweep so both CSVs
    // cover at least 3 full cycles of their respective time-base.
    const double sweepDurationMs = std::max(durationMs, feasibleDurationMs);
    const unsigned long totalMs =
        static_cast<unsigned long>(std::ceil(3.0 * sweepDurationMs));
    const unsigned long stepMs = 10;

    const std::string csvPath = analysisDir + "/spline_samples_" + id + ".csv";
    const std::string feasibleCsvPath =
        analysisDir + "/spline_samples_" + id + "_feasible.csv";
    FILE* csv = fopen(csvPath.c_str(), "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(csv, ("failed to open " + csvPath).c_str());
    fprintf(csv, "millis_ms,t,position,handleY,velocity,acceleration\n");

    FILE* feasibleCsv = fopen(feasibleCsvPath.c_str(), "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(
        feasibleCsv, ("failed to open " + feasibleCsvPath).c_str());
    fprintf(feasibleCsv,
            "millis_ms,t,position,handleY,velocity,acceleration,jerk\n");

    for (unsigned long ms = 0; ms <= totalMs; ms += stepMs) {
        g_fakedMs = ms;

        // Mirror evaluate()'s internal t for CSV labeling. evaluate() itself
        // recomputes this from millis(), so the test and production code see
        // the same t.
        const double t =
            std::fmod(static_cast<double>(ms), durationMs) / durationMs;

        SplineSample s = pattern.evaluate(1.0);
        SplineSample fs = pattern.evaluateFeasible(1.0);

        fprintf(csv, "%lu,%.9f,%.9f,%.9f,%.9f,%.9f\n", ms, t, s.position,
                s.handleY, s.velocity, s.acceleration);
        fprintf(feasibleCsv, "%lu,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n", ms, fs.t,
                fs.position, fs.handleY, fs.velocity, fs.acceleration, fs.jerk);

        if (s.position > result.maxPos) result.maxPos = s.position;
        if (s.position < result.minPos) result.minPos = s.position;

        const double velMm = std::fabs(fs.velocity) * kTestPlayRangeMm;
        const double accMm = std::fabs(fs.acceleration) * kTestPlayRangeMm;
        const double jrkMm = std::fabs(fs.jerk) * kTestPlayRangeMm;
        if (velMm > result.maxFeasibleVelMm) result.maxFeasibleVelMm = velMm;
        if (accMm > result.maxFeasibleAccMm) result.maxFeasibleAccMm = accMm;
        if (jrkMm > result.maxFeasibleJerkMm) result.maxFeasibleJerkMm = jrkMm;

        if (ms == 0) result.t0 = s;
        if (ms == static_cast<unsigned long>(durationMs)) result.tDuration = s;
    }

    fclose(csv);
    fclose(feasibleCsv);
    printf(
        "Wrote %s (totalDuration=%.3fs, feasibleDuration=%.3fs, %lu ms swept; "
        "peak feasible v=%.1f mm/s, a=%.1f mm/s^2, j=%.1f mm/s^3)\n",
        csvPath.c_str(), pattern.totalDuration(),
        pattern.feasibleTotalDuration(), totalMs, result.maxFeasibleVelMm,
        result.maxFeasibleAccMm, result.maxFeasibleJerkMm);
    return result;
}

// Sweep a single pattern with two speed changes mid-stroke, sharing the
// underlying fake clock across all three phases. Captures 500 samples at
// ~100 Hz (10 ms spacing, 5 s total): 10% -> 50% at the 33% mark, then
// 50% -> 10% at the 66% mark. SplinePattern is constructed once, so its
// internal startTime is captured once and every phase transition reflects
// a real mid-period speed change.
static void sweepPatternWithSpeedChange(const std::string& id,
                                        const std::string& json,
                                        const std::string& analysisDir) {
    resetFakeClock();
    SplinePattern pattern;
    const bool ok = pattern.loadFromJson(
        id.c_str(), json.c_str(), kTestPlayRangeMm, kTestMaxSpeedMmPerSec,
        kTestMaxAccelMmPerSec2, kTestMaxJerkMmPerSec3);
    TEST_ASSERT_TRUE_MESSAGE(ok, ("loadFromJson failed for " + id).c_str());

    const unsigned long stepMs = 10;
    const unsigned long numSamples = 500;
    const unsigned long switchUpSample = numSamples / 3;        // 33%
    const unsigned long switchDownSample = (2 * numSamples) / 3;  // 66%
    const unsigned long switchUpMs = switchUpSample * stepMs;
    const unsigned long switchDownMs = switchDownSample * stepMs;

    const std::string csvPath =
        analysisDir + "/spline_samples_" + id + "_speedchange.csv";
    FILE* csv = fopen(csvPath.c_str(), "w");
    TEST_ASSERT_NOT_NULL_MESSAGE(csv, ("failed to open " + csvPath).c_str());
    // Extra `speed` column lets the plotter draw markers at each change.
    // plot_spline.py's parse_csv uses names=True so older readers that only
    // look up the original 5 columns still work.
    fprintf(csv, "millis_ms,t,position,handleY,velocity,acceleration,speed\n");

    for (unsigned long i = 0; i < numSamples; ++i) {
        const unsigned long ms = i * stepMs;
        g_fakedMs = ms;

        double speedPercent;
        if (i < switchUpSample) {
            speedPercent = 0.10;
        } else if (i < switchDownSample) {
            speedPercent = 0.50;
        } else {
            speedPercent = 0.10;
        }

        SplineSample s = pattern.evaluate(speedPercent);

        fprintf(csv, "%lu,%.9f,%.9f,%.9f,%.9f,%.9f,%.3f\n", ms, s.t, s.position,
                s.handleY, s.velocity, s.acceleration, s.speedPercent);
    }

    fclose(csv);
    printf(
        "Wrote %s (totalDuration=%.3fs, %lu samples @ %lums, switches at "
        "%lums [10%%->50%%] and %lums [50%%->10%%])\n",
        csvPath.c_str(), pattern.totalDuration(), numSamples, stepMs,
        switchUpMs, switchDownMs);
}

// ─── Tests ───────────────────────────────────────────────────────────────

static std::string g_patternDir;
static std::string g_analysisDir;
static std::vector<std::string> g_ids;

void test_pattern_dir_resolves() {
    TEST_ASSERT_FALSE_MESSAGE(g_patternDir.empty(),
                              "could not locate data/patterns/ from test cwd");
    TEST_ASSERT_FALSE(g_ids.empty());
    printf("patternDir=%s (%u patterns)\n", g_patternDir.c_str(),
           (unsigned)g_ids.size());
}

void test_sweep_all_patterns() {
    // Sample-grid mis-alignment with the calibration sweep can let the
    // empirical peak skim a few % past the analytic bound; allow that wiggle
    // before failing the test.
    constexpr double kBoundSlack = 1.10;

    for (const auto& id : g_ids) {
        const std::string path = g_patternDir + "/" + id + ".json";
        const std::string json = slurp(path);
        TEST_ASSERT_FALSE_MESSAGE(json.empty(),
                                  ("failed to read " + path).c_str());

        SweepResult r = sweepPattern(id, json, g_analysisDir);

        // Periodicity: position at t=0 and t=totalDuration should match
        // within a tight tolerance (spline is periodic by construction).
        TEST_ASSERT_DOUBLE_WITHIN_MESSAGE(
            1e-6, r.t0.position, r.tDuration.position,
            ("periodicity broken for " + id).c_str());

        // Sanity: raw spline should stay near [0, 1]. Allow small Hermite
        // overshoot.
        TEST_ASSERT_TRUE_MESSAGE(r.minPos > -0.25,
                                 ("unexpected undershoot in " + id).c_str());
        TEST_ASSERT_TRUE_MESSAGE(r.maxPos < 1.25,
                                 ("unexpected overshoot in " + id).c_str());

        // Feasibility: the secondary spline (evaluateFeasible) must keep
        // physical |v| and |a| below the limits passed to loadFromJson at
        // speedPercent = 1.0. These are bounded for any cubic Bezier path
        // (first/second derivatives are finite), so a tight assertion is
        // appropriate.
        TEST_ASSERT_TRUE_MESSAGE(
            r.maxFeasibleVelMm <= kTestMaxSpeedMmPerSec * kBoundSlack,
            ("feasible spline exceeded MAX_SPEED for " + id).c_str());
        TEST_ASSERT_TRUE_MESSAGE(
            r.maxFeasibleAccMm <= kTestMaxAccelMmPerSec2 * kBoundSlack,
            ("feasible spline exceeded MAX_ACCEL for " + id).c_str());

        // Jerk is left advisory — cubic Beziers are at best C² across
        // segment boundaries, so patterns with tangent discontinuities
        // (corners) have impulsive |d³y/dx³| there. Uniform time-scaling
        // cannot bound that; only path-smoothing or a runtime jerk-limited
        // tracker could. The peak is printed by sweepPattern() so it's
        // visible in the test log for diagnostic purposes.
    }
}

// For each pattern, run one continuous sweep with two speed changes
// (10% -> 50% at 33% of the timeline, 50% -> 10% at 66%), sharing a single
// fake clock so each change happens mid-period.
// Output: Analysis/spline_samples_<id>_speedchange.csv
void test_sweep_all_patterns_with_speed_change() {
    for (const auto& id : g_ids) {
        const std::string path = g_patternDir + "/" + id + ".json";
        const std::string json = slurp(path);
        TEST_ASSERT_FALSE_MESSAGE(json.empty(),
                                  ("failed to read " + path).c_str());
        sweepPatternWithSpeedChange(id, json, g_analysisDir);
    }
}

int runUnityTests() {
    UNITY_BEGIN();
    RUN_TEST(test_pattern_dir_resolves);
    RUN_TEST(test_sweep_all_patterns);
    RUN_TEST(test_sweep_all_patterns_with_speed_change);
    return UNITY_END();
}

int main(void) {
    g_patternDir = resolvePatternDir();
    g_analysisDir = resolveAnalysisDir();
    if (!g_patternDir.empty()) g_ids = listJsonIds(g_patternDir);
    return runUnityTests();
}
