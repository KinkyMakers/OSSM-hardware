#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"

void OSSM::drawPatternControlsTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    SettingPercents savedSettings = OSSM::setting;

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine.pattern"_s);
    };

    StrokePatterns nextPattern = OSSM::setting.pattern;
    bool shouldUpdateDisplay = true;
    String patternName;
    String patternDescription;

    // Change encode
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setBoundaries(0, static_cast<int>(StrokePatterns::Count) * 3 - 1, true);
    ossm->encoder.setEncoderValue((int)nextPattern * 3);

    while (isInCorrectState(ossm)) {
        float speed;
        float speedKnob = getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        OSSM::setting.speedKnob = speedKnob;
        if (USE_SPEED_KNOB_AS_LIMIT || !OSSM::setting.speedBLE.has_value()) {
            speed = speedKnob * (OSSM::setting.speedBLE.value_or(100)) / 100;
        } else {
            speedKnob = OSSM::setting.speedBLE.value_or(100);
            speed = OSSM::setting.speedBLE.value_or(100);
        }

        if (speed != OSSM::setting.speed) {
            shouldUpdateDisplay = true;
            OSSM::setting.speed = speed;
        }

        nextPattern = StrokePatterns(ossm->encoder.readEncoder() / 3);
        shouldUpdateDisplay = shouldUpdateDisplay || OSSM::setting.pattern != nextPattern;
        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }

        patternName = UserConfig::language.StrokeEngineNames[(int)nextPattern];
        patternDescription = UserConfig::language.StrokeEngineDescriptions[(int)nextPattern];
        OSSM::setting.pattern = nextPattern;

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearPage(true, true);

            // Draw the title
            drawStr::title(patternName);
            drawStr::multiLine(0, 20, patternDescription);
            drawShape::scroll(100 * (int)nextPattern /(int)StrokePatterns::Count);

            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(200);
    }

    vTaskDelete(nullptr);
};

void OSSM::drawPatternControls() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPatternControlsTask, "drawPatternControlsTask", stackSize,
                this, 1, &Tasks::drawPatternControlsTaskH);
}
