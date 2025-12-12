#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"

size_t numberOfDescriptions =
    sizeof(UserConfig::language.StrokeEngineDescriptions) /
    sizeof(UserConfig::language.StrokeEngineDescriptions[0]);
size_t numberOfPatterns = 9;

void OSSM::drawPatternControlsTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    SettingPercents savedSettings = OSSM::setting;

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine.pattern"_s);
    };

    int nextPattern = (int)OSSM::setting.pattern;
    bool shouldUpdateDisplay = true;
    const char *patternName = "nextPattern";
    const char *patternDescription =
        UserConfig::language.StrokeEngineDescriptions[nextPattern];

    // Change encode
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setBoundaries(0, numberOfPatterns * 3 - 1, true);
    ossm->encoder.setEncoderValue(nextPattern * 3);

    while (isInCorrectState(ossm)) {
        nextPattern = ossm->encoder.readEncoder() / 3;
        shouldUpdateDisplay =
            shouldUpdateDisplay || (int)OSSM::setting.pattern != nextPattern;
        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }

        patternName = UserConfig::language.StrokeEngineNames[nextPattern];

        if (nextPattern < numberOfDescriptions) {
            patternDescription =
                UserConfig::language.StrokeEngineDescriptions[nextPattern];
        } else {
            patternDescription = "No description available";
        }

        OSSM::setting.pattern = (StrokePatterns)nextPattern;

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearPage(true, true);

            // Draw the title
            drawStr::title(patternName);
            drawStr::multiLine(0, 20, patternDescription);
            drawShape::scroll(100 * nextPattern / numberOfPatterns);

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
