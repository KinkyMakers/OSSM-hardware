#include "play_controls.h"

#include "constants/Config.h"
#include "services/board.h"
#include "constants/Menu.h"
#include "constants/Pins.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/state/menu.h"
#include "ossm/state/session.h"
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

namespace play_controls {

static void drawPlayControlsTask(void *pvParameters) {
    encoder.setAcceleration(10);
    encoder.setBoundaries(0, 100, false);
    // Clean up!
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

    auto menuString = menuStrings[menuState.currentOption];

    SettingPercents next = {0, 0, 0, 0, 0};
    unsigned long displayLastUpdated = 0;

    /**
     * /////////////////////////////////////////////
     * /////////// Play Controls Display ///////////
     * /////////////////////////////////////////////
     *
     * This is a safety feature to prevent the user from accidentally beginning
     * a session at max speed. After the user decreases the speed to 0.5% or
     * less, the state machine will be allowed to continue.
     */
    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("simplePenetration"_s) ||
               stateMachine->is("simplePenetration.idle"_s) ||
               stateMachine->is("strokeEngine"_s) ||
               stateMachine->is("strokeEngine.idle"_s) ||
               stateMachine->is("streaming"_s) ||
               stateMachine->is("streaming.idle"_s);
    };

    // Line heights
    short lh3 = 56;
    short lh4 = 64;
    static float encoderValue = 0;

    bool isStrokeEngine = stateMachine->is("strokeEngine"_s) ||
                          stateMachine->is("strokeEngine.idle"_s);
    
    bool isStreaming = stateMachine->is("streaming"_s) ||
                       stateMachine->is("streaming.idle"_s);

    bool shouldUpdateDisplay = false;

    // This small break gives the encoder a minute to settle.
    vTaskDelay(100);

    String headerText = "";

    while (isInCorrectState()) {
        // Always assume the display should not update.
        shouldUpdateDisplay = false;

#ifdef AJ_DEVELOPMENT_HARDWARE
        next.speedKnob = 0;
#else
        next.speedKnob =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
#endif
        settings.speedKnob = next.speedKnob;
        encoderValue = encoder.readEncoder();

        if (USE_SPEED_KNOB_AS_LIMIT || !settings.speedBLE.has_value()) {
            next.speed = next.speedKnob * (settings.speedBLE.value_or(100)) / 100;
        } else {
            next.speedKnob = settings.speedBLE.value_or(100);
            next.speed = settings.speedBLE.value_or(100);
        }

        if (next.speed != settings.speed) {
            shouldUpdateDisplay = true;
            settings.speed = next.speed;
        }

        switch (session.playControl) {
            case PlayControls::STROKE:
                next.stroke = encoderValue;
                shouldUpdateDisplay =
                    shouldUpdateDisplay || next.stroke - settings.stroke >= 1;
                settings.stroke = next.stroke;
                break;
            case PlayControls::SENSATION:
                next.sensation = encoderValue;
                shouldUpdateDisplay =
                    shouldUpdateDisplay || next.sensation - settings.sensation >= 1;
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
                    shouldUpdateDisplay || next.buffer - settings.buffer >=1;
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

        String strokeString = UserConfig::language.Stroke;
        auto stringWidth = display.getUTF8Width(strokeString.c_str());

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            // Check and update the header text... don't worry if this is the
            // same as last time, nothing happens.
            if (isStrokeEngine) {
                headerText = UserConfig::language.StrokeEngineNames[(int)settings.pattern];
            } else if (isStreaming){
                headerText = UserConfig::language.Streaming;
            } else {
                headerText = UserConfig::language.SimplePenetration;
            }
            setHeader(headerText);

            // Now draw the page...
            clearPage(true);
            display.setFont(Config::Font::base);

            if (!isStreaming){
                drawShape::settingBar(UserConfig::language.Speed, next.speedKnob);
            } else {
                drawShape::settingBar("Max", next.speedKnob);
                display.setFont(Config::Font::bold);
                display.drawUTF8(14, 36, UserConfig::language.Speed);
            }

            if (isStrokeEngine) {
                switch (session.playControl) {
                    case PlayControls::STROKE:
                        drawShape::settingBarSmall(settings.sensation, 125);
                        drawShape::settingBarSmall(settings.depth, 120);
                        drawShape::settingBar(strokeString, settings.stroke,
                                              118, 0, RIGHT_ALIGNED);
                        break;
                    case PlayControls::SENSATION:
                        drawShape::settingBar(F("Sensation"), settings.sensation,
                                              128, 0, RIGHT_ALIGNED, 10);
                        drawShape::settingBarSmall(settings.depth, 113);
                        drawShape::settingBarSmall(settings.stroke, 108);
                        break;
                    case PlayControls::DEPTH:
                        drawShape::settingBarSmall(settings.sensation, 125);
                        drawShape::settingBar(F("Depth"), settings.depth, 123,
                                              0, RIGHT_ALIGNED, 5);
                        drawShape::settingBarSmall(settings.stroke, 108);
                        break;
                    default:
                        break;
                }
            } else if (isStreaming) {
                switch (session.playControl) {
                    case PlayControls::BUFFER:
                        drawShape::settingBar(F("Buffer"),
                                              settings.buffer, 128, 0,
                                              RIGHT_ALIGNED, 15);
                        drawShape::settingBarSmall(settings.sensation, 113);
                        drawShape::settingBarSmall(settings.depth, 108);
                        drawShape::settingBarSmall(settings.stroke, 103);
                        break;
                    case PlayControls::STROKE:
                        drawShape::settingBarSmall(settings.buffer, 125);
                        drawShape::settingBarSmall(settings.sensation, 120);
                        drawShape::settingBarSmall(settings.depth, 115);
                        drawShape::settingBar(strokeString,
                                              settings.stroke, 113, 0,
                                              RIGHT_ALIGNED);
                        break;
                    case PlayControls::SENSATION:
                        drawShape::settingBarSmall(settings.buffer, 125);
                        drawShape::settingBar(F("Max"),
                                              settings.sensation, 123, 0,
                                              RIGHT_ALIGNED, 10);
                        display.setFont(Config::Font::bold);
                        strokeString = String("Accel");
                        stringWidth = display.getUTF8Width(strokeString.c_str());
                        display.drawUTF8(99 - stringWidth, 36, strokeString.c_str());
                        drawShape::settingBarSmall(settings.depth, 108);
                        drawShape::settingBarSmall(settings.stroke, 103);
                        break;
                    case PlayControls::DEPTH:
                        drawShape::settingBarSmall(settings.buffer, 125);
                        drawShape::settingBarSmall(settings.sensation, 120);
                        drawShape::settingBar(F("Depth"), settings.depth,
                                              118, 0, RIGHT_ALIGNED, 5);
                        drawShape::settingBarSmall(settings.stroke, 103);
                        break;
                }
            } else {
                drawShape::settingBar(strokeString, encoder.readEncoder(), 118,
                                      0, RIGHT_ALIGNED);
            }

            /**
             * /////////////////////////////////////////////
             * /////////// Play Controls Left  ////////////
             * /////////////////////////////////////////////
             *
             * These controls are associated with stroke and distance
             */
            if (!isStrokeEngine && !isStreaming) {
                display.setFont(Config::Font::small);
                strokeString = "# " + String(session.strokeCount);
                display.drawUTF8(14, lh4, strokeString.c_str());
            }
            /**
             * /////////////////////////////////////////////
             * /////////// Play Controls Right  ////////////
             * /////////////////////////////////////////////
             *
             * These controls are associated with stroke and distance
             */

            if (!isStrokeEngine && !isStreaming) {
                strokeString = formatDistance(session.distanceMeters);
                stringWidth = display.getUTF8Width(strokeString.c_str());
                display.drawUTF8(104 - stringWidth, lh3, strokeString.c_str());
            }
            if (!isStreaming){
                strokeString =
                    formatTime(displayLastUpdated - session.startTime).c_str();
                stringWidth = display.getUTF8Width(strokeString.c_str());
                display.drawUTF8(104 - stringWidth, lh4, strokeString.c_str());
            }

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
