#include "pattern_controls.h"

#include "constants/Pins.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/pattern_registry.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "ui.h"
#include "utils/AnalogSampler.h"
#include "utils/format.h"
#include "components/HeaderBar.h"

namespace sml = boost::sml;
using namespace sml;

namespace pattern_controls {

static void drawPatternControlsTask(void *pvParameters) {
    SettingPercents savedSettings = settings;

    auto isInCorrectState = []() {
        return stateMachine->is("strokeEngine.pattern"_s);
    };

    int nextPattern = settings.pattern;
    bool shouldUpdateDisplay = true;
    const char *patternName = patternCatalog[nextPattern].name;
    const char *patternDescription = patternCatalog[nextPattern].description;

    encoder.setAcceleration(10);
    encoder.setBoundaries(0, totalPatternCount * 3 - 1, true);
    encoder.setEncoderValue(nextPattern * 3);

    showHeaderIcons = true;

    while (isInCorrectState()) {
        float speed;
        float speedKnob =
            AnalogSampler::readPercent(Pins::Remote::speedPotPin);
        settings.speedKnob = speedKnob;
        if (USE_SPEED_KNOB_AS_LIMIT || !settings.speedBLE.has_value()) {
            speed = speedKnob * settings.speedBLE.value_or(100.0f) / 100.0f;
        } else {
            speedKnob = settings.speedBLE.value_or(100.0f);
            speed = settings.speedBLE.value_or(100.0f);
        }

        if (speed != settings.speed) {
            shouldUpdateDisplay = true;
            settings.speed = speed;
        }

        nextPattern = encoder.readEncoder() / 3;
        shouldUpdateDisplay =
            shouldUpdateDisplay || settings.pattern != nextPattern;
        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }

        patternName = patternCatalog[nextPattern].name;
        patternDescription = patternCatalog[nextPattern].description;

        settings.pattern = nextPattern;

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::TextPage page;
            page.title = patternName;
            page.body = patternDescription;
            page.scrollPercent =
                ui::scrollPercent(nextPattern, totalPatternCount);
            ui::drawTextPage(display.getU8g2(), page);
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
