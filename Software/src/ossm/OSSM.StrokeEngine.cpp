#include "OSSM.h"

#include "services/stepper.h"

void OSSM::startStrokeEngineTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    float measuredStrokeMm = ossm->measuredStrokeSteps / (1_mm);

    machineGeometry strokingMachine = {
        .physicalTravel = abs(ossm->measuredStrokeSteps / (1_mm)),
        .keepoutBoundary = 6.0};
    SettingPercents lastSetting = ossm->setting;

    Stroker.begin(&strokingMachine, &servoMotor, ossm->stepper);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(ossm->setting.sensation), true);

    Stroker.setDepth(0.01f * ossm->setting.depth * abs(measuredStrokeMm), true);
    Stroker.setStroke(0.01f * ossm->setting.stroke * abs(measuredStrokeMm),
                      true);
    Stroker.moveToMax(10 * 3);

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s) ||
               ossm->sm->is("strokeEngine.pattern"_s);
    };

    while (isInCorrectState(ossm)) {
        if (isChangeSignificant(lastSetting.speed, ossm->setting.speed)) {
            if (ossm->setting.speed == 0) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(ossm->setting.speed * 3, true);
            lastSetting.speed = ossm->setting.speed;
        }

        if (lastSetting.stroke != ossm->setting.stroke) {
            float newStroke =
                0.01f * ossm->setting.stroke * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", ossm->setting.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = ossm->setting.stroke;
        }

        if (lastSetting.depth != ossm->setting.depth) {
            float newDepth =
                0.01f * ossm->setting.depth * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change depth: %f %f", ossm->setting.depth,
                     newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = ossm->setting.depth;
        }

        if (lastSetting.sensation != ossm->setting.sensation) {
            float newSensation = calculateSensation(ossm->setting.sensation);
            ESP_LOGD("UTILS", "change sensation: %f %f",
                     ossm->setting.sensation, newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = ossm->setting.sensation;
        }

        if (lastSetting.pattern != ossm->setting.pattern) {
            ESP_LOGD("UTILS", "change pattern: %d", ossm->setting.pattern);

            switch (ossm->setting.pattern) {
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

            lastSetting.pattern = ossm->setting.pattern;
        }

        vTaskDelay(400);
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void OSSM::startStrokeEngine() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, this, configMAX_PRIORITIES - 1,
                            &runStrokeEngineTaskH, operationTaskCore);
}