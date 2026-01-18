#include "OSSM.h"

#include <mqtt_client.h>
#include <services/communication/mqtt.h>

#include "services/stepper.h"
#include "utils/getEfuseMac.h"

void OSSM::startStrokeEngineTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    float measuredStrokeMm = ossm->measuredStrokeSteps / (1_mm);

    machineGeometry strokingMachine = {
        .physicalTravel = abs(ossm->measuredStrokeSteps / (1_mm)),
        .keepoutBoundary = 6.0};
    SettingPercents lastSetting = OSSM::setting;

    Stroker.begin(&strokingMachine, &servoMotor, ossm->stepper);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(OSSM::setting.sensation), true);

    Stroker.setDepth(0.01f * OSSM::setting.depth * abs(measuredStrokeMm), true);
    Stroker.setStroke(0.01f * OSSM::setting.stroke * abs(measuredStrokeMm),
                      true);

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s) ||
               ossm->sm->is("strokeEngine.pattern"_s);
    };

    unsigned long lastPublishTime = 0;
    const unsigned long publishInterval = 100;  // 1 second

    String lastPayload = "";
    String currentPayload = "";

    while (isInCorrectState(ossm)) {
        if (isChangeSignificant(lastSetting.speed, OSSM::setting.speed) ||
            ossm->wasLastSpeedCommandFromBLE(true)) {
            // Speed is float, so give a little wiggle room here to assume 0
            if (OSSM::setting.speed < 0.1f) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(OSSM::setting.speed * 3, true);
            lastSetting.speed = OSSM::setting.speed;
            vTaskDelay(pdMS_TO_TICKS(100));
        }

        if (lastSetting.stroke != OSSM::setting.stroke) {
            float newStroke =
                0.01f * OSSM::setting.stroke * abs(measuredStrokeMm);
            // ESP_LOGD("UTILS", "change stroke: %f %f", OSSM::setting.stroke,
            //          newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = OSSM::setting.stroke;
        }

        if (lastSetting.depth != OSSM::setting.depth) {
            float newDepth =
                0.01f * OSSM::setting.depth * abs(measuredStrokeMm);
            // ESP_LOGD("UTILS", "change depth: %f %f", OSSM::setting.depth,
            //          newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = OSSM::setting.depth;
        }

        if (lastSetting.sensation != OSSM::setting.sensation) {
            float newSensation = calculateSensation(OSSM::setting.sensation);
            // ESP_LOGD("UTILS", "change sensation: %f %f",
            //          OSSM::setting.sensation, newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = OSSM::setting.sensation;
        }

        if (lastSetting.pattern != OSSM::setting.pattern) {
            // ESP_LOGD("UTILS", "change pattern: %d", OSSM::setting.pattern);

            switch (OSSM::setting.pattern) {
                case StrokePatterns::SimpleStroke:
                    Stroker.setPattern(new SimpleStroke("Simple Stroke"),
                                       false);
                    break;
                case StrokePatterns::TeasingPounding:
                    Stroker.setPattern(new TeasingPounding("Teasing Pounding"),
                                       false);
                    break;
                case StrokePatterns::RoboStroke:
                    Stroker.setPattern(new RoboStroke("Robo Stroke"), false);
                    break;
                case StrokePatterns::HalfnHalf:
                    Stroker.setPattern(new HalfnHalf("Half'n'Half"), false);
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
                default:
                    break;
            }

            lastSetting.pattern = OSSM::setting.pattern;
        }

        vTaskDelay(100);
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void OSSM::startStrokeEngine() {
    ESP_LOGD("OSSM", "Starting stroke engine");

    int stackSize = 20 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, this, configMAX_PRIORITIES - 1,
                            &Tasks::runStrokeEngineTaskH,
                            Tasks::operationTaskCore);

    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            OSSM *ossm = (OSSM *)pvParameters;

            auto isInCorrectState = [](OSSM *ossm) {
                return ossm->sm->is("strokeEngine"_s) ||
                       ossm->sm->is("strokeEngine.idle"_s) ||
                       ossm->sm->is("strokeEngine.pattern"_s);
            };

            String lastPayload = "";
            String currentPayload = "";

            while (isInCorrectState(ossm)) {
                // time between publishes

                if (!mqttConnected) {
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    continue;
                }

                vTaskDelay(pdMS_TO_TICKS(100));

                currentPayload = ossm->getCurrentState();

                int result = esp_mqtt_client_publish(
                    mqttClient, String("ossm/" + getMacAddress()).c_str(),
                    currentPayload.c_str(), currentPayload.length(), 0, false);

                if (result < 0) {
                    ESP_LOGD("OSSM", "Publish failed: %d", result);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
            }

            vTaskDelete(nullptr);
        },
        "publishStateTask", 5 * configMINIMAL_STACK_SIZE, this,
        tskIDLE_PRIORITY + 1, nullptr, Tasks::operationTaskCore);
}
