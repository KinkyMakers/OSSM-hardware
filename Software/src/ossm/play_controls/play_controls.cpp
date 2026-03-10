#include "play_controls.h"

#include "Strings.h"
#include "constants/Menu.h"
#include "constants/Pins.h"
#include "services/board.h"
#include "ossm/state/ble.h"
#include "ossm/state/menu.h"
#include "ossm/state/session.h"
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

namespace play_controls {

static ui::PlayControl toUiPlayControl(PlayControls pc) {
    switch (pc) {
        case PlayControls::STROKE:    return ui::PlayControl::STROKE;
        case PlayControls::SENSATION: return ui::PlayControl::SENSATION;
        case PlayControls::DEPTH:     return ui::PlayControl::DEPTH;
        case PlayControls::BUFFER:    return ui::PlayControl::BUFFER;
    }
    return ui::PlayControl::STROKE;
}

static void drawPlayControlsTask(void *pvParameters) {
    encoder.setAcceleration(10);
    encoder.setBoundaries(0, 100, false);

    switch (session.playControl) {
        case PlayControls::STROKE:
            encoder.setEncoderValue(settings.stroke);
            break;
        case PlayControls::SENSATION:
            encoder.setEncoderValue(settings.sensation);
            break;
        case PlayControls::DEPTH:
            encoder.setEncoderValue(settings.depth);
            break;
        case PlayControls::BUFFER:
            encoder.setEncoderValue(settings.buffer);
            break;
    }

    SettingPercents next = {0, 0, 0, 0, 0};
    unsigned long displayLastUpdated = 0;

    auto isInCorrectState = []() {
        return stateMachine->is("simplePenetration"_s) ||
               stateMachine->is("simplePenetration.idle"_s) ||
               stateMachine->is("strokeEngine"_s) ||
               stateMachine->is("strokeEngine.idle"_s) ||
               stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.idle"_s);
    };

    static float encoderValue = 0;

    bool isStrokeEngine = stateMachine->is("strokeEngine"_s) ||
                          stateMachine->is("strokeEngine.idle"_s);

    bool isStreaming = stateMachine->is("streaming"_s) ||
                       stateMachine->is("streaming.idle"_s);

    bool shouldUpdateDisplay = false;

    vTaskDelay(100);

    while (isInCorrectState()) {
        shouldUpdateDisplay = false;

#ifdef AJ_DEVELOPMENT_HARDWARE
        next.speedKnob = 0;
#else
        next.speedKnob =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
#endif
        if (abs(next.speedKnob - settings.speedKnob) > 2 &&
            next.speedKnob < settings.speed) {
            ::resetLastSpeedCommandWasFromBLE();
        }

        next.speed = next.speedKnob;
        if (settings.speedBLE.has_value()) {
            if (USE_SPEED_KNOB_AS_LIMIT) {
                next.speed = next.speedKnob * settings.speedBLE.value() / 100;
            } else if (::wasLastSpeedCommandFromBLE()) {
                next.speed = settings.speedBLE.value();
            }
        }

        if (next.speed != settings.speed) {
            shouldUpdateDisplay = true;
            settings.speed = next.speed;
        }

        settings.speedKnob = next.speedKnob;
        encoderValue = encoder.readEncoder();

        switch (session.playControl) {
            case PlayControls::STROKE:
                next.stroke = encoderValue;
                shouldUpdateDisplay =
                    shouldUpdateDisplay || next.stroke - settings.stroke >= 1;
                settings.stroke = next.stroke;
                break;
            case PlayControls::SENSATION:
                next.sensation = encoderValue;
                shouldUpdateDisplay = shouldUpdateDisplay ||
                                      next.sensation - settings.sensation >= 1;
                settings.sensation = next.sensation;
                break;
            case PlayControls::DEPTH:
                next.depth = encoderValue;
                shouldUpdateDisplay =
                    shouldUpdateDisplay || next.depth - settings.depth >= 1;
                settings.depth = next.depth;
                break;
            case PlayControls::BUFFER:
                next.buffer = encoderValue;
                shouldUpdateDisplay =
                    shouldUpdateDisplay || next.buffer - settings.buffer >= 1;
                settings.buffer = next.buffer;
                break;
        }

        shouldUpdateDisplay =
            shouldUpdateDisplay || millis() - displayLastUpdated > 1000;

        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }

        displayLastUpdated = millis();

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            const char *headerText;
            if (isStrokeEngine) {
                headerText =
                    ui::strings::strokeEngineNames[(int)settings.pattern];
            } else if (isStreaming) {
                headerText = ui::strings::streaming;
            } else {
                headerText = ui::strings::simplePenetration;
            }

            static String distStr;
            static String timeStr;
            if (!isStrokeEngine && !isStreaming) {
                distStr = formatDistance(session.distanceMeters);
            }
            if (!isStreaming) {
                timeStr = formatTime(displayLastUpdated - session.startTime);
            }

            ui::PlayControlsData data{};
            data.speed = next.speed;
            data.stroke = settings.stroke;
            data.sensation = settings.sensation;
            data.depth = settings.depth;
            data.buffer = settings.buffer;
            data.activeControl = toUiPlayControl(session.playControl);
            data.strokeCount = session.strokeCount;
            data.distanceMeters = session.distanceMeters;
            data.elapsedMs = displayLastUpdated - session.startTime;
            data.pattern = (int)settings.pattern;
            data.isStrokeEngine = isStrokeEngine;
            data.isStreaming = isStreaming;
            data.headerText = headerText;
            data.speedLabel = ui::strings::speed;
            data.strokeLabel = ui::strings::stroke;
            data.distanceStr =
                (!isStrokeEngine && !isStreaming) ? distStr.c_str() : nullptr;
            data.timeStr = !isStreaming ? timeStr.c_str() : nullptr;

            ui::drawPlayControls(display.getU8g2(), data);
            refreshPage(true);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(200);
    }

    vTaskDelete(nullptr);
};

void drawPlayControls() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPlayControlsTask, "drawPlayControlsTask", stackSize,
                nullptr, 1, &Tasks::drawPlayControlsTaskH);
}

}  // namespace play_controls
