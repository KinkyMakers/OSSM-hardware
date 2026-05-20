// =============================================================================
//                       !! DESTRUCTIVE TEST BUILD !!
//
// SimplePenetration is repurposed as a Toppra motion-control bench test:
//   1. FastAccelStepper is torn down (its planner is gone for good until
//      reboot) and an RMT TX channel takes over the STEP pin directly.
//   2. A cubic-spline chirp covering the full measured rail is built once.
//   3. Toppra (TOPPRA + ConstAccel) produces the time-optimal v/a-limited
//      trajectory through those waypoints.
//   4. A high-priority RT task evaluates the parametrizer at ~200 Hz and
//      emits exactly the right number of step pulses on the STEP pin via
//      RMT bursts.
//
// No UI is drawn while in this mode. Exiting the mode does NOT restore the
// FAS pipeline; the device must be rebooted to use other modes after this.
// =============================================================================

#include "simple_penetration.h"

#include <Arduino.h>
#include <esp_log.h>
#include <math.h>

#include <memory>
#include <vector>

#include "constants/Config.h"
#include "driver/rmt.h"
#include "ossm/state/calibration.h"
#include "ossm/state/state.h"
#include "services/stepper.h"
#include "services/tasks.h"

#include <toppra/algorithm/toppra.hpp>
#include <toppra/constraint/linear_joint_acceleration.hpp>
#include <toppra/constraint/linear_joint_velocity.hpp>
#include <toppra/geometric_path/piecewise_poly_path.hpp>
#include <toppra/parametrizer/const_accel.hpp>

namespace sml = boost::sml;
using namespace sml;

namespace simple_penetration {

static const char *TAG = "ToppraChirp";

// ---------- Tunables ----------------------------------------------------------

// Loop tick: how often we re-evaluate the parametrizer and push pulses.
static constexpr uint32_t kTickMs = 5;           // 200 Hz

// Trajectory planning safety margins (fraction of Config::Driver limits).
static constexpr float kVelocitySafety = 0.50f;  // 50% of max linear velocity
static constexpr float kAccelSafety    = 0.40f;  // 40% of max acceleration

// Spatial chirp shape (cycles per full path traversal, s ∈ [0, 1]).
//   * N_min and N_max must average to an INTEGER so the path closes back
//     on itself (q(0) == q(1)), allowing seamless looping via fmod.
//   * Spatial frequency profile:  f(s) = N_min + (N_max - N_min) * (1 - cos(2π s))/2
//   * Total cycles over s ∈ [0,1]:  (N_min + N_max) / 2
static constexpr int   kCyclesMin = 2;
static constexpr int   kCyclesMax = 8;            // mean = 5 cycles → closed
static constexpr int   kSamplesPerMinCycle = 16;  // density floor

// Toppra discretization (segments). Higher = smoother v(t), slower to solve.
static constexpr int   kToppraN = 400;

// Bound the per-tick pulse emission so a planning glitch can't dump 10k
// pulses into the queue at once (would also blow rmt_item32_t stack buffer).
static constexpr size_t kMaxPulsesPerTick = 256;

// Pulse shape — leave STEP high for ~2us, low for the rest of the period.
static constexpr uint32_t kPulseHighUs = 2;
static constexpr uint32_t kPulseMinPeriodUs = 4;   // ≥ 250 kHz cap

// ------------------------------------------------------------------------------

struct ChirpTrajectory {
    std::shared_ptr<toppra::PiecewisePolyPath> path;
    std::shared_ptr<toppra::parametrizer::ConstAccel> parametrizer;
    double totalDurationSeconds = 0.0;
    double railLengthMm = 0.0;     // usable rail (after keepout)
    double offsetMm = 0.0;         // base offset of motion (keepout)
    double stepsPerMm = 0.0;       // for mm → steps conversion
};

// Spatial frequency envelope (instantaneous spatial frequency in cycles
// per unit s). Smooth bell from kCyclesMin → kCyclesMax → kCyclesMin.
static double instantaneousCycles(double s) {
    return kCyclesMin +
           (kCyclesMax - kCyclesMin) * (1.0 - std::cos(2.0 * M_PI * s)) * 0.5;
}

// Integrated phase  ∫₀ˢ f(u) du. Both q and q' vanish at s=0 and s=1,
// so the spline closes smoothly and natural BCs are well posed.
static double integratedPhase(double s) {
    return kCyclesMin * s +
           0.5 * (kCyclesMax - kCyclesMin) *
               (s - std::sin(2.0 * M_PI * s) / (2.0 * M_PI));
}

static bool buildTrajectory(ChirpTrajectory &out) {
    const double railFullMm =
        std::fabs(calibration.measuredStrokeSteps) / (1_mm);
    const double keepoutMm = 6.0;
    const double railUsableMm = railFullMm - 2.0 * keepoutMm;
    if (railUsableMm <= 20.0) {
        ESP_LOGE(TAG, "Rail too short (%.1f mm usable). Run homing first.",
                 railUsableMm);
        return false;
    }
    out.railLengthMm = railUsableMm;
    out.offsetMm = keepoutMm;
    out.stepsPerMm = (1_mm);  // alias for Config::Driver::stepsPerMM

    // Choose waypoint count so the densest cycle still has ≥ kSamplesPerMinCycle
    // samples per cycle. Min spatial wavelength corresponds to N_max cycles.
    const int totalSamples =
        std::max(64, kCyclesMax * kSamplesPerMinCycle + 1);

    toppra::Vectors positions;
    positions.reserve(totalSamples);
    toppra::Vector times(totalSamples);

    const double amplitude = out.railLengthMm;       // peak-to-peak
    const double midpoint = out.offsetMm + amplitude * 0.5;

    for (int i = 0; i < totalSamples; ++i) {
        const double s = static_cast<double>(i) / (totalSamples - 1);
        const double phi = integratedPhase(s);
        // q(s) = midpoint - amplitude/2 * cos(2π φ) → starts and ends at
        // midpoint - amplitude/2 = offset (one rail extreme), traverses
        // full amplitude every cycle of φ.
        const double q = midpoint - amplitude * 0.5 * std::cos(2.0 * M_PI * phi);

        toppra::Vector p(1);
        p << q;
        positions.push_back(p);
        times(i) = s;
    }

    toppra::BoundaryCond bcNatural{"natural"};
    toppra::BoundaryCondFull bcs{bcNatural, bcNatural};

    out.path = std::make_shared<toppra::PiecewisePolyPath>(
        toppra::PiecewisePolyPath::CubicSpline(positions, times, bcs));

    // --- Constraints --------------------------------------------------------
    const double vMax =
        Config::Driver::maxSpeedMmPerSecond * kVelocitySafety;
    // Config::Driver::maxAcceleration is in *steps*/s² (because of the _mm
    // literal applied in Config.h). Convert back to mm/s².
    const double aMax =
        (Config::Driver::maxAcceleration / (1_mm)) * kAccelSafety;

    ESP_LOGI(TAG, "v_max=%.1f mm/s  a_max=%.1f mm/s²  rail=%.1f mm  cycles=%d..%d",
             vMax, aMax, out.railLengthMm, kCyclesMin, kCyclesMax);

    toppra::Vector vLo(1), vHi(1);
    vLo << -vMax;
    vHi << vMax;
    auto velConstraint =
        std::make_shared<toppra::constraint::LinearJointVelocity>(vLo, vHi);

    toppra::Vector aLo(1), aHi(1);
    aLo << -aMax;
    aHi << aMax;
    auto accConstraint =
        std::make_shared<toppra::constraint::LinearJointAcceleration>(aLo, aHi);

    toppra::LinearConstraintPtrs constraints{velConstraint, accConstraint};

    // --- Solve --------------------------------------------------------------
    toppra::algorithm::TOPPRA algo(constraints, out.path);
    algo.setN(kToppraN);

    const uint32_t t0 = millis();
    toppra::ReturnCode rc = algo.computePathParametrization(0.0, 0.0);
    const uint32_t solveMs = millis() - t0;

    if (rc != toppra::ReturnCode::OK) {
        ESP_LOGE(TAG, "TOPPRA solve failed (%d): %s", static_cast<int>(rc),
                 algo.getErrorMessage().c_str());
        return false;
    }
    ESP_LOGI(TAG, "TOPPRA solved in %u ms (N=%d)", (unsigned)solveMs, kToppraN);

    const auto &data = algo.getParameterizationData();
    out.parametrizer =
        std::make_shared<toppra::parametrizer::ConstAccel>(
            out.path, data.gridpoints, data.parametrization);
    out.totalDurationSeconds = out.parametrizer->pathInterval()[1];

    if (!std::isfinite(out.totalDurationSeconds) ||
        out.totalDurationSeconds <= 0.01) {
        ESP_LOGE(TAG, "Bogus parametrization duration: %f s",
                 out.totalDurationSeconds);
        return false;
    }

    ESP_LOGI(TAG, "Trajectory ready: T_total=%.3f s",
             out.totalDurationSeconds);
    return true;
}

static bool isInCorrectState() {
    return stateMachine->is("simplePenetration"_s) ||
           stateMachine->is("simplePenetration.idle"_s);
}

static int32_t evalTargetSteps(const ChirpTrajectory &traj, double t) {
    // Wrap t into [0, T] so the chirp loops forever.
    double tMod = std::fmod(t, traj.totalDurationSeconds);
    if (tMod < 0) tMod += traj.totalDurationSeconds;
    // Hold the parametrizer just inside its bound to avoid out-of-range throws.
    const double eps = 1e-6;
    if (tMod < eps) tMod = eps;
    if (tMod > traj.totalDurationSeconds - eps)
        tMod = traj.totalDurationSeconds - eps;

    toppra::Vector tv(1);
    tv << tMod;
    auto pos = traj.parametrizer->eval(tv, 0);
    const double posMm = pos[0](0);
    return static_cast<int32_t>(std::lround(posMm * traj.stepsPerMm));
}

static void startToppraChirpTask(void *) {
    // 1. Tear down FAS, bring up raw RMT TX on STEP pin.
    teardownStepperForRawRmt();

    // 2. Plan trajectory once (this throws std::runtime_error on bad input).
    ChirpTrajectory traj;
    bool ok = false;
    try {
        ok = buildTrajectory(traj);
    } catch (const std::exception &e) {
        ESP_LOGE(TAG, "Trajectory build threw: %s", e.what());
        ok = false;
    }
    if (!ok) {
        ESP_LOGE(TAG, "Aborting RT loop. REBOOT REQUIRED to recover stepper.");
        vTaskDelete(nullptr);
        return;
    }

    // 3. RT loop --------------------------------------------------------------
    int32_t lastSteps = evalTargetSteps(traj, 0.0);  // anchor at t=0
    rawStepSetDirection(true);

    const uint32_t t0 = millis();
    uint32_t nextWake = xTaskGetTickCount();
    uint32_t lastLogMs = millis();

    // Reusable per-tick pulse buffer (stack-friendly via local static array).
    static rmt_item32_t items[kMaxPulsesPerTick];

    while (isInCorrectState()) {
        const uint32_t nowMs = millis();
        const double tSec = (nowMs - t0) / 1000.0;

        int32_t targetSteps = 0;
        try {
            targetSteps = evalTargetSteps(traj, tSec);
        } catch (const std::exception &e) {
            ESP_LOGW(TAG, "eval threw at t=%.3f: %s", tSec, e.what());
            vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(kTickMs));
            continue;
        }

        int32_t delta = targetSteps - lastSteps;
        if (delta != 0) {
            const bool forward = delta > 0;
            const uint32_t pulses =
                std::min<uint32_t>(std::abs(delta), kMaxPulsesPerTick);

            // STEP-to-STEP period (µs) — distribute pulses evenly across tick.
            const uint32_t tickUs = kTickMs * 1000;
            uint32_t periodUs = pulses ? (tickUs / pulses) : kPulseMinPeriodUs;
            if (periodUs < kPulseMinPeriodUs) periodUs = kPulseMinPeriodUs;
            const uint32_t lowUs =
                periodUs > kPulseHighUs ? periodUs - kPulseHighUs : 1;

            // Build the RMT item array.
            for (uint32_t i = 0; i < pulses; ++i) {
                items[i].level0 = 1;
                items[i].duration0 = kPulseHighUs;
                items[i].level1 = 0;
                items[i].duration1 = lowUs;
            }
            rawStepSetDirection(forward);
            rawStepWritePulses(items, pulses);

            lastSteps += forward ? static_cast<int32_t>(pulses)
                                 : -static_cast<int32_t>(pulses);
        }

        if (nowMs - lastLogMs > 500) {
            ESP_LOGI(TAG,
                     "t=%.3fs target=%ld actual=%ld delta=%ld T_total=%.3f",
                     tSec, (long)targetSteps, (long)lastSteps, (long)delta,
                     traj.totalDurationSeconds);
            lastLogMs = nowMs;
        }

        vTaskDelayUntil(&nextWake, pdMS_TO_TICKS(kTickMs));
    }

    ESP_LOGW(TAG, "Exiting RT loop. REBOOT REQUIRED before other modes.");
    vTaskDelete(nullptr);
}

void startSimplePenetration() {
    // Generous stack — Toppra/Eigen plus exception machinery is heavy.
    int stackSize = 24 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startToppraChirpTask, "toppraChirpTask", stackSize,
                            nullptr, configMAX_PRIORITIES - 1,
                            &Tasks::runSimplePenetrationTaskH,
                            Tasks::operationTaskCore);
}

}  // namespace simple_penetration
