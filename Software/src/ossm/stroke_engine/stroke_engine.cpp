#include "stroke_engine.h"

#include <mqtt_client.h>

#include "SplinePattern.h"
#include "constants/Config.h"
#include "constants/UserConfig.h"
#include "ossm/OSSM.h"
#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/communication/mqtt.h"
#include "services/pattern_registry.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "utils/StrokeEngineHelper.h"
#include "utils/getEfuseMac.h"

namespace sml = boost::sml;
using namespace sml;

namespace stroke_engine {

static void startStrokeEngineTask(void *pvParameters) {
    float measuredStrokeMm = calibration.measuredStrokeSteps / (1_mm);

    machineGeometry strokingMachine = {
        .physicalTravel = abs(calibration.measuredStrokeSteps / (1_mm)),
        .keepoutBoundary = 6.0};
    SettingPercents lastSetting = settings;

    Stroker.begin(&strokingMachine, &servoMotor, stepper);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(settings.sensation), true);

    Stroker.setDepth(0.01f * settings.depth * abs(measuredStrokeMm), true);
    Stroker.setStroke(0.01f * settings.stroke * abs(measuredStrokeMm), true);

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("strokeEngine"_s) ||
               stateMachine->is("strokeEngine.idle"_s) ||
               stateMachine->is("strokeEngine.pattern"_s);
    };

    while (isInCorrectState()) {
        if (isChangeSignificant(lastSetting.speed, settings.speed) ||
            wasLastSpeedCommandFromBLE()) {
            // Speed is float, so give a little wiggle room here to assume 0
            if (settings.speed < 0.1f) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(settings.speed * 3, true);
            lastSetting.speed = settings.speed;
        }

        if (lastSetting.stroke != settings.stroke) {
            float newStroke = 0.01f * settings.stroke * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", settings.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = settings.stroke;
        }

        if (lastSetting.depth != settings.depth) {
            float newDepth = 0.01f * settings.depth * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change depth: %f %f", settings.depth, newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = settings.depth;
        }

        if (lastSetting.sensation != settings.sensation) {
            float newSensation = calculateSensation(settings.sensation);
            ESP_LOGD("UTILS", "change sensation: %f %f", settings.sensation,
                     newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = settings.sensation;
        }

        if (lastSetting.pattern != settings.pattern) {
            ESP_LOGD("UTILS", "change pattern: %d", settings.pattern);

            if (settings.pattern < HARDCODED_PATTERN_COUNT) {
                switch (static_cast<StrokePatterns>(settings.pattern)) {
                    case StrokePatterns::SimpleStroke:
                        Stroker.setPattern(new SimpleStroke("Simple Stroke"),
                                           false);
                        break;
                    case StrokePatterns::TeasingPounding:
                        Stroker.setPattern(
                            new TeasingPounding("Teasing Pounding"), false);
                        break;
                    case StrokePatterns::RoboStroke:
                        Stroker.setPattern(new RoboStroke("Robo Stroke"),
                                           false);
                        break;
                    case StrokePatterns::HalfnHalf:
                        Stroker.setPattern(new HalfnHalf("Half'n'Half"),
                                           false);
                        break;
                    case StrokePatterns::Deeper:
                        Stroker.setPattern(new Deeper("Deeper"), false);
                        break;
                    case StrokePatterns::StopNGo:
                        Stroker.setPattern(new StopNGo("Stop'n'Go"), false);
                        break;
                    case StrokePatterns::Insist:
                        Stroker.setPattern(new Insist("Insist"), false);
                        break;
                }
            } else {
                const char *patternId =
                    patternCatalog[settings.pattern].id;
                if (patternId == nullptr) {
                    ESP_LOGE("StrokeEngine",
                             "Registry pattern %d has no id, skipping",
                             settings.pattern);
                    lastSetting.pattern = settings.pattern;
                    continue;
                }

                Stroker.stopMotion();

                SplinePattern spline;
                if (!spline.loadFromFile(patternId)) {
                    ESP_LOGE("StrokeEngine",
                             "Failed to load spline pattern '%s'", patternId);
                    lastSetting.pattern = settings.pattern;
                    continue;
                }

                lastSetting.pattern = settings.pattern;

                float strokeSteps =
                    fabsf(calibration.measuredStrokeSteps);
                float maxSpeedHz =
                    Config::Driver::maxSpeedMmPerSecond * (1_mm);
                float maxAccel =
                    Config::Driver::maxAcceleration * (1_mm);

                TickType_t originTick = xTaskGetTickCount();
                const TickType_t tickIntervalMs = 100;

                while (isInCorrectState()) {
                    if (lastSetting.pattern != settings.pattern) break;

                    bool isStopped =
                        settings.speed <
                        Config::Advanced::commandDeadZonePercentage;

                    if (isStopped) {
                        stepper->stopMove();
                        originTick = xTaskGetTickCount();
                        vTaskDelay(pdMS_TO_TICKS(tickIntervalMs));
                        continue;
                    }

                    float timeScale = 100.0f / settings.speed;
                    float wallDtSec =
                        (float)(xTaskGetTickCount() - originTick) /
                        (float)configTICK_RATE_HZ;
                    float splineTime = wallDtSec / timeScale;

                    float duration = spline.totalDuration();
                    if (duration > 0) {
                        splineTime = fmodf(splineTime, duration);
                        if (splineTime < 0) splineTime += duration;
                    }

                    float posNorm = spline.evaluate(splineTime);
                    posNorm = fmaxf(0.0f, fminf(1.0f, posNorm));

                    float velNorm = spline.evaluateVelocity(splineTime);
                    float velScaled = velNorm / timeScale;

                    int32_t targetPos =
                        (int32_t)(posNorm * strokeSteps * 0.5f);
                    float velStepsPerSec =
                        fabsf(velScaled) * strokeSteps * 0.5f;
                    velStepsPerSec =
                        fmaxf(100.0f, fminf(velStepsPerSec, maxSpeedHz));

                    float accelSteps =
                        velStepsPerSec /
                        (tickIntervalMs / 1000.0f);
                    accelSteps =
                        fmaxf(1000.0f, fminf(accelSteps, maxAccel));

                    stepper->setSpeedInHz((uint32_t)velStepsPerSec);
                    stepper->setAcceleration((int32_t)accelSteps);
                    stepper->moveTo(targetPos, false);

                    ESP_LOGV("SplineCtrl",
                             "t=%.3f pos=%.3f vel=%.1f target=%d",
                             splineTime, posNorm, velStepsPerSec,
                             targetPos);

                    vTaskDelay(pdMS_TO_TICKS(tickIntervalMs));
                }

                stepper->stopMove();

                if (!isInCorrectState()) break;
                continue;
            }

            lastSetting.pattern = settings.pattern;
        }

        if (bleState.hasActiveConnection) {
            vTaskDelay(100);
        } else {
            vTaskDelay(400);
        }
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

static void publishStateTask(void *pvParameters) {
    auto isInCorrectState = []() {
        return stateMachine->is("strokeEngine"_s) ||
               stateMachine->is("strokeEngine.idle"_s) ||
               stateMachine->is("strokeEngine.pattern"_s);
    };

    const TickType_t publishInterval = pdMS_TO_TICKS(
        (int)(1000.0f / UserConfig::mqttPublishFrequencyHz));

    TickType_t lastWakeTime = xTaskGetTickCount();

    while (isInCorrectState()) {
        if (!mqttConnected) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            lastWakeTime = xTaskGetTickCount();
            continue;
        }

        vTaskDelayUntil(&lastWakeTime, publishInterval);

        String payload = ossm->getCurrentState();
        String topic = "ossm/" + getMacAddress();

        int result = esp_mqtt_client_publish(
            mqttClient, topic.c_str(), payload.c_str(), payload.length(), 0,
            false);

        if (result < 0) {
            ESP_LOGD("MQTT", "Publish failed: %d", result);
            vTaskDelay(pdMS_TO_TICKS(1000));
            lastWakeTime = xTaskGetTickCount();
        }
    }

    vTaskDelete(nullptr);
}

void startStrokeEngine() {
    int stackSize = 12 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, nullptr, configMAX_PRIORITIES - 1,
                            &Tasks::runStrokeEngineTaskH,
                            Tasks::operationTaskCore);

    xTaskCreatePinnedToCore(publishStateTask, "publishStateTask",
                            5 * configMINIMAL_STACK_SIZE, nullptr,
                            tskIDLE_PRIORITY + 1, nullptr,
                            Tasks::operationTaskCore);
}

}  // namespace stroke_engine
