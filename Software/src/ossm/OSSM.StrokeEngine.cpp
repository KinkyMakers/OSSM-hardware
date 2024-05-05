#include "OSSM.h"

void OSSM::startStrokeEngineTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    ossm->stepper.stopService();

    machineGeometry strokingMachine = {
        .physicalTravel = abs(ossm->measuredStrokeMm), .keepoutBoundary = 6.0};
    SettingPercents lastSetting = ossm->setting;

    class StrokeEngine Stroker;

    Stroker.begin(&strokingMachine, &servoMotor);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(ossm->setting.sensation), true);

    Stroker.setPattern(int(ossm->setting.pattern), true);
    Stroker.setDepth(0.01f * ossm->setting.depth * abs(ossm->measuredStrokeMm),
                     true);
    Stroker.setStroke(
        0.01f * ossm->setting.stroke * abs(ossm->measuredStrokeMm), true);
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
                0.01f * ossm->setting.stroke * abs(ossm->measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", ossm->setting.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = ossm->setting.stroke;
        }

        if (lastSetting.depth != ossm->setting.depth) {
            float newDepth =
                0.01f * ossm->setting.depth * abs(ossm->measuredStrokeMm);
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
            Stroker.setPattern(ossm->setting.pattern, false);
            lastSetting.pattern = ossm->setting.pattern;
        }

        vTaskDelay(400);
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void OSSM::startStrokeEngine() {
    int stackSize = 15 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, this, configMAX_PRIORITIES - 1,
                            &runStrokeEngineTaskH, operationTaskCore);
}