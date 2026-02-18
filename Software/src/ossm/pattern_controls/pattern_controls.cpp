#include "pattern_controls.h"

#include "constants/Pins.h"
#include "services/board.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "utils/analog.h"
#include "utils/format.h"

namespace sml = boost::sml;
using namespace sml;

namespace pattern_controls {

static size_t numberOfDescriptions =
    sizeof(UserConfig::language.StrokeEngineDescriptions) /
    sizeof(UserConfig::language.StrokeEngineDescriptions[0]);
static size_t numberOfPatterns = 7;

static void drawPatternControlsTask(void *pvParameters) {
    SettingPercents savedSettings = settings;

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("strokeEngine.pattern"_s);
    };

    int nextPattern = (int)settings.pattern;
    bool shouldUpdateDisplay = true;
    const char *patternName = "nextPattern";
    const char *patternDescription =
        UserConfig::language.StrokeEngineDescriptions[nextPattern];

    // Change encode
    encoder.setAcceleration(10);
    encoder.setBoundaries(0, numberOfPatterns * 3 - 1, true);
    encoder.setEncoderValue(nextPattern * 3);

    while (isInCorrectState()) {
        float speed;
        float speedKnob =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        settings.speedKnob = speedKnob;
        if (USE_SPEED_KNOB_AS_LIMIT || !settings.speedBLE.has_value()) {
            speed = speedKnob * (settings.speedBLE.value_or(100)) / 100;
        } else {
            speedKnob = settings.speedBLE.value_or(100);
            speed = settings.speedBLE.value_or(100);
        }

        if (speed != settings.speed) {
            shouldUpdateDisplay = true;
            settings.speed = speed;
        }

        nextPattern = encoder.readEncoder() / 3;
        shouldUpdateDisplay =
            shouldUpdateDisplay || (int)settings.pattern != nextPattern;
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

        settings.pattern = (StrokePatterns)nextPattern;

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

void drawPatternControls() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPatternControlsTask, "drawPatternControlsTask", stackSize,
                nullptr, 1, &Tasks::drawPatternControlsTaskH);
}

}  // namespace pattern_controls
