#include "pattern_controls.h"

#include "Strings.h"
#include "constants/Pins.h"
#include "services/board.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "ui.h"
#include "utils/analog.h"
#include "utils/format.h"

namespace sml = boost::sml;
using namespace sml;

namespace pattern_controls {

static size_t numberOfDescriptions =
    sizeof(ui::strings::strokeEngineDescriptions) /
    sizeof(ui::strings::strokeEngineDescriptions[0]);
static size_t numberOfPatterns = 
    sizeof(ui::strings::strokeEngineNames) /
    sizeof(ui::strings::strokeEngineNames[0]);

static void drawPatternControlsTask(void *pvParameters) {
    SettingPercents savedSettings = settings;

    auto isInCorrectState = []() {
        return stateMachine->is("strokeEngine.pattern"_s);
    };

    StrokePatterns nextPattern = settings.pattern;
    bool shouldUpdateDisplay = true;
    const char *patternName = "nextPattern";
    const char *patternDescription =
        ui::strings::strokeEngineDescriptions[(int)nextPattern];

    encoder.setAcceleration(10);
    encoder.setBoundaries(0, numberOfPatterns * 3 - 1, true);
    encoder.setEncoderValue((int)nextPattern * 3);

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

        nextPattern = StrokePatterns(encoder.readEncoder() / 3);
        shouldUpdateDisplay = shouldUpdateDisplay || settings.pattern != nextPattern;
        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }

        patternName = ui::strings::strokeEngineNames[(int)nextPattern];

        if ((int)nextPattern < (int)numberOfDescriptions) {
            patternDescription =
                ui::strings::strokeEngineDescriptions[(int)nextPattern];
        } else {
            patternDescription = ui::strings::noDescription;
        }

        settings.pattern = nextPattern;

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::TextPage page;
            page.title = patternName;
            page.body = patternDescription;
            page.scrollPercent = ui::scrollPercent((int)nextPattern, numberOfPatterns);
            ui::drawTextPage(display.getU8g2(), page);
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