#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "services/tasks.h"
#include "utils/analog.h"
#include "utils/format.h"

void OSSM::drawPlayControlsTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setBoundaries(0, 100, false);
    // Clean up!
    switch (ossm->playControl) {
        case PlayControls::STROKE:
            ossm->encoder.setEncoderValue(ossm->setting.stroke);
            break;
        case PlayControls::SENSATION:
            ossm->encoder.setEncoderValue(ossm->setting.sensation);
            break;
        case PlayControls::DEPTH:
            ossm->encoder.setEncoderValue(ossm->setting.depth);
            break;
    }

    auto menuString = menuStrings[ossm->menuOption];

    SettingPercents next = {0, 0, 0, 0};
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
    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("simplePenetration"_s) ||
               ossm->sm->is("simplePenetration.idle"_s) ||
               ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s);
    };

    // Line heights
    short lh3 = 56;
    short lh4 = 64;
    static float encoder = 0;

    bool isStrokeEngine =
        ossm->sm->is("strokeEngine"_s) || ossm->sm->is("strokeEngine.idle"_s);

    bool shouldUpdateDisplay = false;

    // This small break gives the encoder a minute to settle.
    vTaskDelay(100);

    while (isInCorrectState(ossm)) {
        // Always assume the display should not update.
        shouldUpdateDisplay = false;

        next.speedKnob =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        ossm->setting.speedKnob = next.speedKnob;
        encoder = ossm->encoder.readEncoder();

        next.speed = next.speedKnob;

        if (next.speed != ossm->setting.speed) {
            shouldUpdateDisplay = true;
            ossm->setting.speed = next.speed;
        }

        switch (ossm->playControl) {
            case PlayControls::STROKE:
                next.stroke = encoder;
                shouldUpdateDisplay = shouldUpdateDisplay ||
                                      next.stroke - ossm->setting.stroke >= 1;
                ossm->setting.stroke = next.stroke;
                break;
            case PlayControls::SENSATION:
                next.sensation = encoder;
                shouldUpdateDisplay =
                    shouldUpdateDisplay ||
                    next.sensation - ossm->setting.sensation >= 1;
                ossm->setting.sensation = next.sensation;
                break;
            case PlayControls::DEPTH:
                next.depth = encoder;
                shouldUpdateDisplay = shouldUpdateDisplay ||
                                      next.depth - ossm->setting.depth >= 1;
                ossm->setting.depth = next.depth;
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
        auto stringWidth = ossm->display.getUTF8Width(strokeString.c_str());

        displayMutex.lock();
        ossm->display.clearBuffer();
        ossm->display.setFont(Config::Font::base);

        drawShape::settingBar(UserConfig::language.Speed, next.speedKnob);

        if (isStrokeEngine) {
            switch (ossm->playControl) {
                case PlayControls::STROKE:
                    drawShape::settingBarSmall(ossm->setting.sensation, 125);
                    drawShape::settingBarSmall(ossm->setting.depth, 120);
                    drawShape::settingBar(strokeString, ossm->setting.stroke,
                                          118, 0, RIGHT_ALIGNED);
                    break;
                case PlayControls::SENSATION:
                    drawShape::settingBar("Sensation", ossm->setting.sensation,
                                          128, 0, RIGHT_ALIGNED, 10);
                    drawShape::settingBarSmall(ossm->setting.depth, 113);
                    drawShape::settingBarSmall(ossm->setting.stroke, 108);

                    break;
                case PlayControls::DEPTH:
                    drawShape::settingBarSmall(ossm->setting.sensation, 125);
                    drawShape::settingBar("Depth", ossm->setting.depth, 123, 0,
                                          RIGHT_ALIGNED, 5);
                    drawShape::settingBarSmall(ossm->setting.stroke, 108);

                    break;
            }
        } else {
            drawShape::settingBar(strokeString, ossm->encoder.readEncoder(),
                                  118, 0, RIGHT_ALIGNED);
        }

        /**
         * /////////////////////////////////////////////
         * /////////// Play Controls Left  ////////////
         * /////////////////////////////////////////////
         *
         * These controls are associated with stroke and distance
         */
        ossm->display.setFont(Config::Font::small);
        strokeString = "# " + String(ossm->sessionStrokeCount);
        ossm->display.drawUTF8(14, lh4, strokeString.c_str());

        /**
         * /////////////////////////////////////////////
         * /////////// Play Controls Right  ////////////
         * /////////////////////////////////////////////
         *
         * These controls are associated with stroke and distance
         */

        if (!isStrokeEngine) {
            strokeString = formatDistance(ossm->sessionDistanceMeters);
            stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
            ossm->display.drawUTF8(104 - stringWidth, lh3,
                                   strokeString.c_str());
        }

        strokeString =
            formatTime(displayLastUpdated - ossm->sessionStartTime).c_str();
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(104 - stringWidth, lh4, strokeString.c_str());

        ossm->display.sendBuffer();
        displayMutex.unlock();

        vTaskDelay(200);
    }

    vTaskDelete(nullptr);
};

void OSSM::drawPlayControls() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPlayControlsTask, "drawPlayControlsTask", stackSize, this,
                1, &drawPlayControlsTaskH);
}