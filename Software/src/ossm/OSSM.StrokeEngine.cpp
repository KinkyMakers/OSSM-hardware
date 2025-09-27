#include "OSSM.h"

#include "services/stepper.h"

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

    while (isInCorrectState(ossm)) {
        if (isChangeSignificant(lastSetting.speed, OSSM::setting.speed) || ossm->wasLastSpeedCommandFromBLE(true)) {
            //Speed is float, so give a little wiggle room here to assume 0
            if (OSSM::setting.speed < 0.1f) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(OSSM::setting.speed * 3, true);
            lastSetting.speed = OSSM::setting.speed;
        }

        if (lastSetting.stroke != OSSM::setting.stroke) {
            float newStroke =
                0.01f * OSSM::setting.stroke * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", OSSM::setting.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = OSSM::setting.stroke;
        }

        if (lastSetting.depth != OSSM::setting.depth) {
            float newDepth =
                0.01f * OSSM::setting.depth * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change depth: %f %f", OSSM::setting.depth,
                     newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = OSSM::setting.depth;
        }

        if (lastSetting.sensation != OSSM::setting.sensation) {
            float newSensation = calculateSensation(OSSM::setting.sensation);
            ESP_LOGD("UTILS", "change sensation: %f %f",
                     OSSM::setting.sensation, newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = OSSM::setting.sensation;
        }

        if (lastSetting.pattern != OSSM::setting.pattern) {
            ESP_LOGD("UTILS", "change pattern: %d", OSSM::setting.pattern);

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

        if(ossm->hasActiveBLEConnection) {
            // When connected to BLE, update more frequently for improved responsiveness
            vTaskDelay(100);
        } else {
            vTaskDelay(400);
        }
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void OSSM::startStrokeEngine() {
    int stackSize = 12 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, this, configMAX_PRIORITIES - 1,
                            &Tasks::runStrokeEngineTaskH,
                            Tasks::operationTaskCore);
}
