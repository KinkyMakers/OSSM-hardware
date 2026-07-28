/**
 * Open-loop Streaming stress suite.
 *
 * Full run: pio test -e hw_test -f test_hw_streaming_stress
 * Four-case run: pio test -e hw_streaming_quick
 *
 * WARNING: This suite homes the machine and commands approximately nine
 * minutes of bounded motion. Clear the rail, remove attachments, keep the
 * emergency stop accessible, and leave the physical speed knob at zero.
 *
 * The same catalog is consumed by R+D Motion Lab's closed-loop runner. This
 * on-device variant self-injects the native `stream:position:time` commands,
 * samples FastAccelStepper's reported position, and emits one JSON result per
 * condition for host-side collection.
 */

#include <Arduino.h>
#include <unity.h>

#include <cmath>
#include <algorithm>
#include <limits>
#include <numeric>

#include "constants/Menu.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"
#include "services/stepper.h"
#include "streaming_logic.h"
#include "streaming_stress_suite.h"

namespace sml = boost::sml;
using namespace sml;
using namespace streaming_stress;

namespace {

constexpr uint32_t kHomingTimeoutMs = 30000;
constexpr size_t kSampleCapacity =
    (kDurationMs + kAlignmentGuardMs) / kSamplePeriodMs + 2;

struct Sample {
    int32_t requestedSteps;
    int32_t measuredSteps;
};

struct CaseResult {
    float rawRmseMm;
    float alignedRmseMm;
    int32_t lagMs;
    int32_t measuredRangeSteps;
    size_t sampleCount;
    bool safe;
    bool smoothnessValid;
    float jitterPowerPercent;
    float accelerationRmsMmPerSecondSquared;
    float accelerationP95MmPerSecondSquared;
    float jerkRmsMmPerSecondCubed;
    float jerkP95MmPerSecondCubed;
    float amplitudeRatio;
    float waveformCorrelation;
    float minimumBoundaryMarginMm;
    float maximumBoundaryViolationMm;
    float timeOutsideLegalIntervalSeconds;
};

Sample samples[kSampleCapacity];
constexpr size_t kFFTSize = 4096;
float fftReal[kFFTSize];
float fftImag[kFFTSize];

constexpr float kSGVelocity[31] = {
    0.0120392731873f, 0.00520743488011f, -0.000376978579884f,
    -0.0048030689675f, -0.00815993805754f, -0.0105366876248f,
    -0.0120224194441f, -0.0127062352902f, -0.0126772369379f,
    -0.012024526162f, -0.0108372047373f, -0.00920437443862f,
    -0.00721513704076f, -0.0049585943185f, -0.00252384804665f,
    0.0f, 0.00252384804665f, 0.0049585943185f, 0.00721513704076f,
    0.00920437443862f, 0.0108372047373f, 0.012024526162f,
    0.0126772369379f, 0.0127062352902f, 0.0120224194441f,
    0.0105366876248f, 0.00815993805754f, 0.0048030689675f,
    0.000376978579884f, -0.00520743488011f, -0.0120392731873f};
constexpr float kSGJerk[31] = {
    -0.000754700707262f, -0.000452820424357f, -0.000202988466091f,
    -0.00000148709499f, 0.000155401426422f, 0.000271394835617f,
    0.00035021087007f, 0.000395567267255f, 0.000411181764646f,
    0.000400772099719f, 0.000368056009946f, 0.000316751232802f,
    0.000250575505761f, 0.000173246566298f, 0.000088482151886f,
    0.0f, -0.000088482151886f, -0.000173246566298f,
    -0.000250575505761f, -0.000316751232802f, -0.000368056009946f,
    -0.000400772099719f, -0.000411181764646f, -0.000395567267255f,
    -0.00035021087007f, -0.000271394835617f, -0.000155401426422f,
    0.00000148709499f, 0.000202988466091f, 0.000452820424357f,
    0.000754700707262f};
constexpr float kSGAcceleration[31] = {
    0.00183284457478f, 0.00146627565982f, 0.00112498735969f,
    0.000808979674386f, 0.000518252603903f, 0.000252806148246f,
    0.0000126403074123f, -0.000202244918596f, -0.000391849529781f,
    -0.00055617352614f, -0.000695216907675f, -0.000808979674386f,
    -0.000897461826272f, -0.000960663363333f, -0.00099858428557f,
    -0.00101122459298f, -0.00099858428557f, -0.000960663363333f,
    -0.000897461826272f, -0.000808979674386f, -0.000695216907675f,
    -0.00055617352614f, -0.000391849529781f, -0.000202244918596f,
    0.0000126403074123f, 0.000252806148246f, 0.000518252603903f,
    0.000808979674386f, 0.00112498735969f, 0.00146627565982f,
    0.00183284457478f};

void fft4096() {
    for (size_t index = 1, reversed = 0; index < kFFTSize; ++index) {
        size_t bit = kFFTSize >> 1;
        for (; reversed & bit; bit >>= 1) reversed ^= bit;
        reversed ^= bit;
        if (index < reversed) {
            std::swap(fftReal[index], fftReal[reversed]);
            std::swap(fftImag[index], fftImag[reversed]);
        }
    }
    for (size_t length = 2; length <= kFFTSize; length <<= 1) {
        const float angle = -2.0f * PI / length;
        const float rootReal = std::cos(angle);
        const float rootImag = std::sin(angle);
        for (size_t base = 0; base < kFFTSize; base += length) {
            float wReal = 1.0f;
            float wImag = 0.0f;
            for (size_t offset = 0; offset < length / 2; ++offset) {
                const size_t even = base + offset;
                const size_t odd = even + length / 2;
                const float oddReal = fftReal[odd] * wReal - fftImag[odd] * wImag;
                const float oddImag = fftReal[odd] * wImag + fftImag[odd] * wReal;
                fftReal[odd] = fftReal[even] - oddReal;
                fftImag[odd] = fftImag[even] - oddImag;
                fftReal[even] += oddReal;
                fftImag[even] += oddImag;
                const float nextReal = wReal * rootReal - wImag * rootImag;
                wImag = wReal * rootImag + wImag * rootReal;
                wReal = nextReal;
            }
        }
    }
}

bool waitForMenu() {
    uint32_t started = millis();
    while (!stateMachine->is("menu.idle"_s)) {
        if (millis() - started > kHomingTimeoutMs) return false;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return true;
}

bool waitForStreaming() {
    uint32_t started = millis();
    while (!stateMachine->is("streaming.idle"_s)) {
        if (millis() - started > 5000) return false;
        vTaskDelay(pdMS_TO_TICKS(25));
    }
    return true;
}

void sendCommand(uint8_t position, uint16_t durationMs) {
    String command = "stream:" + String(position) + ":" + String(durationMs);
    ossm->ble_click(command);
}

void applyProfile(const Profile& profile) {
    ossm->ble_click("set:speed:0");
    vTaskDelay(pdMS_TO_TICKS(250));
    ossm->ble_click("set:depth:" + String(profile.depth));
    ossm->ble_click("set:stroke:" + String(profile.stroke));
    ossm->ble_click("set:sensation:" + String(profile.sensation));
    ossm->ble_click("set:buffer:" + String(profile.buffer));
    USE_LATENCY_COMPENSATION = profile.latencyCompensation;
    USE_SPEED_KNOB_AS_LIMIT = false;
}

uint16_t commandInterval(const Scenario& scenario, uint32_t commandIndex) {
    if (scenario.delivery == Delivery::Irregular) {
        return irregularIntervalMs(commandIndex);
    }
    if (scenario.delivery == Delivery::CadencePressure) return 33;
    return regularIntervalMs(scenario);
}

CaseResult analyzeSamples(size_t count, const Profile& profile) {
    const int lagSamples = static_cast<int>(kMaximumLagMs / kSamplePeriodMs);
    const size_t settleEnd = kSettlingMs / kSamplePeriodMs;
    const size_t scoreStart = settleEnd;
    const size_t scoreEnd = kDurationMs / kSamplePeriodMs;
    float bestRmse = std::numeric_limits<float>::infinity();
    float rawRmse = std::numeric_limits<float>::infinity();
    int bestShift = 0;
    double bestOffset = 0;
    int32_t measuredMinimum = std::numeric_limits<int32_t>::max();
    int32_t measuredMaximum = std::numeric_limits<int32_t>::min();

    for (int shift = -lagSamples; shift <= lagSamples; ++shift) {
        double offsetTotal = 0;
        size_t offsetCount = 0;
        for (size_t index = scoreStart; index <= scoreEnd; ++index) {
            int measuredIndex = static_cast<int>(index) + shift;
            if (measuredIndex < 0 || measuredIndex >= static_cast<int>(count)) continue;
            offsetTotal += samples[measuredIndex].measuredSteps - samples[index].requestedSteps;
            ++offsetCount;
        }
        if (offsetCount == 0) continue;
        double offset = offsetTotal / offsetCount;
        double squared = 0;
        size_t scored = 0;
        for (size_t index = scoreStart; index <= scoreEnd; ++index) {
            int measuredIndex = static_cast<int>(index) + shift;
            if (measuredIndex < 0 || measuredIndex >= static_cast<int>(count)) continue;
            double error = samples[measuredIndex].measuredSteps -
                           samples[index].requestedSteps - offset;
            squared += error * error;
            ++scored;
        }
        if (scored == 0) continue;
        float rmse = std::sqrt(squared / scored) / Config::Driver::stepsPerMM;
        if (shift == 0) rawRmse = rmse;
        if (rmse < bestRmse) {
            bestRmse = rmse;
            bestShift = shift;
            bestOffset = offset;
        }
    }

    for (size_t index = scoreStart; index <= scoreEnd && index < count; ++index) {
        measuredMinimum = std::min(measuredMinimum, samples[index].measuredSteps);
        measuredMaximum = std::max(measuredMaximum, samples[index].measuredSteps);
    }
    const int32_t maxStroke = streaming_logic::calculateMaxStroke(
        profile.stroke, profile.depth, calibration.measuredStrokeSteps);
    const int32_t depth = streaming_logic::calculateDepthOffset(
        calibration.measuredStrokeSteps, maxStroke, profile.depth);
    const float legalA = streaming_logic::scaleStreamPosition(0, maxStroke, depth);
    const float legalB = streaming_logic::scaleStreamPosition(100, maxStroke, depth);
    const float legalMinimum = std::min(legalA, legalB) / Config::Driver::stepsPerMM;
    const float legalMaximum = std::max(legalA, legalB) / Config::Driver::stepsPerMM;

    const size_t scoredCount = scoreEnd - scoreStart + 1;
    double measuredSum = 0, requestedSum = 0;
    double measuredSquared = 0, requestedSquared = 0, cross = 0;
    float alignedMinimum = std::numeric_limits<float>::infinity();
    float alignedMaximum = -std::numeric_limits<float>::infinity();
    float requestedMinimum = std::numeric_limits<float>::infinity();
    float requestedMaximum = -std::numeric_limits<float>::infinity();
    float minimumMargin = std::numeric_limits<float>::infinity();
    float maximumViolation = 0;
    size_t outsideCount = 0;
    for (size_t local = 0; local < scoredCount; ++local) {
        const size_t requestedIndex = scoreStart + local;
        const int measuredIndex = static_cast<int>(requestedIndex) + bestShift;
        const float measured =
            (samples[measuredIndex].measuredSteps - bestOffset) /
            Config::Driver::stepsPerMM;
        const float requested = samples[requestedIndex].requestedSteps /
                                Config::Driver::stepsPerMM;
        fftReal[local] = measured;
        alignedMinimum = std::min(alignedMinimum, measured);
        alignedMaximum = std::max(alignedMaximum, measured);
        requestedMinimum = std::min(requestedMinimum, requested);
        requestedMaximum = std::max(requestedMaximum, requested);
        const float physical = samples[requestedIndex].measuredSteps /
                               Config::Driver::stepsPerMM;
        const float margin = std::min(physical - legalMinimum,
                                      legalMaximum - physical);
        minimumMargin = std::min(minimumMargin, margin);
        const float violation = std::max(
            0.0f, std::max(legalMinimum - physical, physical - legalMaximum));
        maximumViolation = std::max(maximumViolation, violation);
        outsideCount += violation > 0;
        measuredSum += measured;
        requestedSum += requested;
        measuredSquared += measured * measured;
        requestedSquared += requested * requested;
        cross += measured * requested;
    }

    const float sampleRate = 1000.0f / kSamplePeriodMs;
    double velocitySquared = 0;
    double accelerationSquared = 0;
    for (size_t local = 0; local < scoredCount; ++local) {
        double velocity = 0;
        double acceleration = 0;
        for (int tap = -15; tap <= 15; ++tap) {
            const size_t source = static_cast<size_t>(std::max<int>(
                0, std::min<int>(scoredCount - 1,
                                 static_cast<int>(local) + tap)));
            const int measuredIndex = static_cast<int>(scoreStart + source) +
                                      bestShift;
            const float position =
                (samples[measuredIndex].measuredSteps - bestOffset) /
                Config::Driver::stepsPerMM;
            velocity += kSGVelocity[tap + 15] * position * sampleRate;
            acceleration += kSGAcceleration[tap + 15] * position *
                            sampleRate * sampleRate;
        }
        fftImag[local] = static_cast<float>(std::abs(acceleration));
        fftReal[local] = static_cast<float>(velocity);
        velocitySquared += velocity * velocity;
        accelerationSquared += acceleration * acceleration;
    }
    const float velocityRms = std::sqrt(velocitySquared / scoredCount);
    const float accelerationRms =
        std::sqrt(accelerationSquared / scoredCount);
    std::sort(fftImag, fftImag + scoredCount);
    const float accelerationP95 = fftImag[std::min(
        scoredCount - 1, static_cast<size_t>(std::ceil(scoredCount * 0.95)) - 1)];

    double jerkSquared = 0;
    for (size_t local = 0; local < scoredCount; ++local) {
        double jerk = 0;
        for (int tap = -15; tap <= 15; ++tap) {
            const size_t source = static_cast<size_t>(std::max<int>(
                0, std::min<int>(scoredCount - 1,
                                 static_cast<int>(local) + tap)));
            const int measuredIndex = static_cast<int>(scoreStart + source) +
                                      bestShift;
            const float position =
                (samples[measuredIndex].measuredSteps - bestOffset) /
                Config::Driver::stepsPerMM;
            jerk += kSGJerk[tap + 15] * position * sampleRate * sampleRate *
                    sampleRate;
        }
        fftImag[local] = static_cast<float>(std::abs(jerk));
        jerkSquared += jerk * jerk;
    }
    const float jerkRms = std::sqrt(jerkSquared / scoredCount);
    std::sort(fftImag, fftImag + scoredCount);
    const float jerkP95 = fftImag[std::min(
        scoredCount - 1, static_cast<size_t>(std::ceil(scoredCount * 0.95)) - 1)];
    const float velocityMean = static_cast<float>(
        std::accumulate(fftReal, fftReal + scoredCount, 0.0) / scoredCount);
    for (size_t index = 0; index < kFFTSize; ++index) {
        if (index < scoredCount) {
            const float hann = 0.5f - 0.5f *
                std::cos(2.0f * PI * index / (scoredCount - 1));
            fftReal[index] = (fftReal[index] - velocityMean) * hann;
        } else {
            fftReal[index] = 0;
        }
        fftImag[index] = 0;
    }
    fft4096();
    double totalPower = 0, jitterPower = 0;
    for (size_t bin = 1; bin <= kFFTSize / 2; ++bin) {
        const float frequency = bin * sampleRate / kFFTSize;
        const double power = fftReal[bin] * fftReal[bin] +
                             fftImag[bin] * fftImag[bin];
        if (frequency >= 0.1f) totalPower += power;
        if (frequency >= 5.0f) jitterPower += power;
    }
    const bool smoothnessValid =
        alignedMaximum - alignedMinimum >= 2.0f && velocityRms >= 1.0f &&
        totalPower > 0;
    const float jitterPercent = smoothnessValid
        ? static_cast<float>(100.0 * jitterPower / totalPower)
        : std::numeric_limits<float>::quiet_NaN();
    const double countDouble = scoredCount;
    const double covariance = cross - measuredSum * requestedSum / countDouble;
    const double measuredVariance = measuredSquared - measuredSum * measuredSum / countDouble;
    const double requestedVariance = requestedSquared - requestedSum * requestedSum / countDouble;
    const float correlation = measuredVariance > 0 && requestedVariance > 0
        ? covariance / std::sqrt(measuredVariance * requestedVariance)
        : 0;
    const float requestedAmplitude = requestedMaximum - requestedMinimum;
    const float amplitudeRatio = requestedAmplitude > 0
        ? (alignedMaximum - alignedMinimum) / requestedAmplitude : 0;

    bool finite = std::isfinite(rawRmse) && std::isfinite(bestRmse);
    bool moved = measuredMaximum - measuredMinimum > 4;
    return {rawRmse, bestRmse, bestShift * static_cast<int32_t>(kSamplePeriodMs),
            measuredMaximum - measuredMinimum, count,
            finite && moved && maximumViolation <= 0.5f,
            smoothnessValid, jitterPercent, accelerationRms, accelerationP95,
            jerkRms, jerkP95,
            amplitudeRatio, correlation, minimumMargin, maximumViolation,
            outsideCount / sampleRate};
}

CaseResult runScenario(const Scenario& scenario) {
    const Profile& profile = *scenario.profile;
    applyProfile(profile);

    uint8_t seedPosition = static_cast<uint8_t>(
        std::lround(requestedPosition(scenario, 0)));
    sendCommand(seedPosition, 100);
    vTaskDelay(pdMS_TO_TICKS(100));
    ossm->ble_click("set:speed:" + String(profile.speed));
    vTaskDelay(pdMS_TO_TICKS(300));

    const uint32_t runDuration = kDurationMs + kAlignmentGuardMs;
    uint32_t started = millis();
    uint32_t nextCommandMs = 0;
    uint32_t nextCommandDueMs = static_cast<uint32_t>(std::max<int32_t>(
        0, deterministicJitterMs(scenario, 0)));
    uint32_t nextSampleMs = 0;
    uint32_t commandIndex = 0;
    uint32_t lastCommandTargetMs = 0;
    size_t sampleCount = 0;
    uint8_t lastPosition = 255;

    while (true) {
        uint32_t elapsed = millis() - started;
        if (elapsed > runDuration) break;

        if (elapsed >= nextSampleMs && sampleCount < kSampleCapacity) {
            int32_t maxStroke = streaming_logic::calculateMaxStroke(
                profile.stroke, profile.depth, calibration.measuredStrokeSteps);
            int32_t depth = streaming_logic::calculateDepthOffset(
                calibration.measuredStrokeSteps, maxStroke, profile.depth);
            uint8_t requested = static_cast<uint8_t>(
                std::lround(requestedPosition(scenario, nextSampleMs)));
            samples[sampleCount++] = {
                streaming_logic::scaleStreamPosition(requested, maxStroke, depth),
                stepper->getCurrentPosition(),
            };
            nextSampleMs += kSamplePeriodMs;
        }

        if (elapsed >= nextCommandDueMs) {
            uint16_t interval = commandInterval(scenario, commandIndex);
            if (scenario.delivery == Delivery::Bursts) {
                for (uint8_t offset = 0; offset < 5; ++offset) {
                    uint32_t targetTime = nextCommandMs + offset * interval;
                    uint8_t position = static_cast<uint8_t>(
                        std::lround(requestedPosition(scenario, targetTime)));
                    uint16_t duration = lastPosition == 255
                        ? interval
                        : static_cast<uint16_t>(std::max<uint32_t>(
                              20, targetTime - lastCommandTargetMs));
                    sendCommand(position, duration);
                    lastPosition = position;
                    lastCommandTargetMs = targetTime;
                }
                nextCommandMs += interval * 5;
                commandIndex += 5;
                nextCommandDueMs = nextCommandMs;
            } else {
                uint8_t position = static_cast<uint8_t>(
                    std::lround(requestedPosition(scenario, nextCommandMs)));
                if (!inDeliveryGap(scenario, nextCommandMs)) {
                    uint16_t duration = lastPosition == 255
                        ? interval
                        : static_cast<uint16_t>(std::max<uint32_t>(
                              20, nextCommandMs - lastCommandTargetMs));
                    if (scenario.delivery == Delivery::Gaps) duration = interval;
                    sendCommand(position, duration);
                    lastPosition = position;
                    lastCommandTargetMs = nextCommandMs;
                }
                nextCommandMs += interval;
                ++commandIndex;
                nextCommandDueMs = static_cast<uint32_t>(std::max<int32_t>(
                    0, static_cast<int32_t>(nextCommandMs) +
                       deterministicJitterMs(scenario, commandIndex)));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ossm->ble_click("set:speed:0");
    vTaskDelay(pdMS_TO_TICKS(300));
    return analyzeSamples(sampleCount, profile);
}

void emitResult(size_t index, const Scenario& scenario, const CaseResult& result) {
    Serial.printf(
        "STREAM_STRESS_RESULT {\"index\":%u,\"id\":\"%s\","
        "\"pattern\":\"%s\",\"profile\":\"%s\",\"repeat\":%s,"
        "\"rawRmseMm\":%.4f,\"alignedRmseMm\":%.4f,\"lagMs\":%ld,"
        "\"measuredRangeMm\":%.4f,\"samples\":%u,\"safe\":%s,"
        "\"smoothnessValid\":%s,\"jitterPowerPercent\":%.6f,"
        "\"accelerationRmsMmPerSecondSquared\":%.4f,"
        "\"accelerationP95MmPerSecondSquared\":%.4f,"
        "\"jerkRmsMmPerSecondCubed\":%.4f,"
        "\"jerkP95MmPerSecondCubed\":%.4f,\"amplitudeRatio\":%.6f,"
        "\"waveformCorrelation\":%.6f,\"minimumBoundaryMarginMm\":%.4f,"
        "\"maximumBoundaryViolationMm\":%.4f,"
        "\"timeOutsideLegalIntervalSeconds\":%.6f}\n",
        static_cast<unsigned>(index), scenario.id, patternName(scenario.pattern),
        scenario.profile->id, scenario.repeat ? "true" : "false",
        result.rawRmseMm, result.alignedRmseMm, static_cast<long>(result.lagMs),
        result.measuredRangeSteps / Config::Driver::stepsPerMM,
        static_cast<unsigned>(result.sampleCount), result.safe ? "true" : "false",
        result.smoothnessValid ? "true" : "false", result.jitterPowerPercent,
        result.accelerationRmsMmPerSecondSquared,
        result.accelerationP95MmPerSecondSquared,
        result.jerkRmsMmPerSecondCubed, result.jerkP95MmPerSecondCubed,
        result.amplitudeRatio, result.waveformCorrelation,
        result.minimumBoundaryMarginMm, result.maximumBoundaryViolationMm,
        result.timeOutsideLegalIntervalSeconds);
}

}  // namespace

void setUp() {}
void tearDown() {}

void test_all_streaming_stress_conditions() {
    TEST_MESSAGE("STREAM_STRESS_PHASE awaiting_menu");
    TEST_ASSERT_TRUE_MESSAGE(waitForMenu(), "OSSM did not home into menu.idle");
    TEST_MESSAGE("STREAM_STRESS_PHASE homing_complete");

    menuState.currentOption = Menu::Streaming;
    stateMachine->process_event(ButtonPress{});
    TEST_ASSERT_TRUE_MESSAGE(waitForStreaming(),
                             "Streaming did not reach idle; keep the speed knob at zero");
    TEST_MESSAGE("STREAM_STRESS_PHASE streaming_ready");

    bool allSafe = true;
#ifdef STREAM_STRESS_QUICK
    constexpr size_t kSelectedCases[] = {0, 8, 16, 25};
    constexpr size_t kSelectedCaseCount =
        sizeof(kSelectedCases) / sizeof(kSelectedCases[0]);
#else
    constexpr size_t kSelectedCaseCount = kCaseCount;
#endif
    for (size_t selection = 0; selection < kSelectedCaseCount; ++selection) {
#ifdef STREAM_STRESS_QUICK
        const size_t index = kSelectedCases[selection];
#else
        const size_t index = selection;
#endif
        Scenario scenario = scenarioAt(index);
        Serial.printf("STREAM_STRESS_CASE {\"index\":%u,\"id\":\"%s\","
                      "\"pattern\":\"%s\",\"profile\":\"%s\"}\n",
                      static_cast<unsigned>(index), scenario.id,
                      patternName(scenario.pattern), scenario.profile->id);
        CaseResult result = runScenario(scenario);
        emitResult(index, scenario, result);
        allSafe = allSafe && result.safe;
    }
    TEST_ASSERT_TRUE_MESSAGE(allSafe,
                             "One or more cases produced invalid or stationary stepper data");
}

void setup() {
    delay(2000);
    initBoard();
    initDisplay();
    ossm = new OSSM();

    UNITY_BEGIN();
    Serial.printf("STREAM_STRESS_BOOT {\"phase\":\"before_state_machine\","
                  "\"freeHeapBytes\":%u}\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.flush();
    initStateMachine();
    Serial.printf("STREAM_STRESS_BOOT {\"phase\":\"state_machine_started\","
                  "\"freeHeapBytes\":%u}\n",
                  static_cast<unsigned>(ESP.getFreeHeap()));
    Serial.flush();

    RUN_TEST(test_all_streaming_stress_conditions);
    ossm->ble_click("set:speed:0");
    stateMachine->process_event(ReturnToMenu{});
    vTaskDelay(pdMS_TO_TICKS(250));
    ossmEmergencyStop();
    UNITY_END();
}

void loop() {}
