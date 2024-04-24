
#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"

void OSSM::drawPlayControlsTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    auto menuString = menuStrings[ossm->menuOption];
    float speedPercentage;
    long strokePercentage;
    int currentTime;

    ossm->speedPercentage = 0;
    ossm->strokePercentage = 0;

    // Set the stepper to the home position
    ossm->stepper.setAccelerationInMillimetersPerSecondPerSecond(1000);
    ossm->stepper.setAccelerationInMillimetersPerSecondPerSecond(10000);
    ossm->stepper.setTargetPositionInMillimeters(0);

    /**
     * /////////////////////////////////////////////
     * //// Safely Block High Speeds on Startup ///
     * /////////////////////////////////////////////
     *
     * This is a safety feature to prevent the user from accidentally beginning
     * a session at max speed. After the user decreases the speed to 0.5% or
     * less, the state machine will be allowed to continue.
     */

    auto isInPreflight = [](OSSM *ossm) {
        // Add your preflight checks states here.
        return ossm->sm->is("simplePenetration.preflight"_s);
    };

    do {
        speedPercentage =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        if (speedPercentage < 0.5) {
            ossm->sm->process_event(Done{});
            break;
        };

        ossm->display.clearBuffer();
        drawStr::title(menuString);
        String speedString = UserConfig::language.Speed + ": " +
                             String((int)speedPercentage) + "%";
        drawStr::centered(25, speedString);
        drawStr::multiLine(0, 40, UserConfig::language.SpeedWarning);

        ossm->display.sendBuffer();
        vTaskDelay(100);
    } while (isInPreflight(ossm));

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
               ossm->sm->is("simplePenetration.idle"_s);
    };

    // Prepare the encoder
    ossm->encoder.setBoundaries(0, 100, false);
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setEncoderValue(0);

    // TODO: prepare the stepper with safe values.

    int16_t y = 64;
    int16_t w = 10;
    int16_t x;
    int16_t h;
    int16_t padding = 4;

    // Line heights
    short lh1 = 25;
    short lh2 = 37;
    short lh3 = 56;
    short lh4 = 64;

    // record session start time rounded to the nearest second
    ossm->sessionStartTime = floor(millis() / 1000);
    ossm->sessionStrokeCount = 0;
    ossm->sessionDistanceMeters = 0;

    bool valueChanged = false;

    while (isInCorrectState(ossm)) {
        speedPercentage = 0.3 * getAnalogAveragePercent(SampleOnPin{
                                    Pins::Remote::speedPotPin, 50}) +
                          0.7 * speedPercentage;
        strokePercentage = ossm->encoder.readEncoder();
        currentTime = floor(millis() / 1000);

        if (ossm->speedPercentage != speedPercentage ||
            ossm->strokePercentage != strokePercentage ||
            ossm->sessionStartTime != currentTime) {
            valueChanged = true;
            ossm->speedPercentage = speedPercentage;
            ossm->strokePercentage = strokePercentage;
        }

        if (!valueChanged) {
            vTaskDelay(100);
            continue;
        }

        ossm->display.clearBuffer();
        ossm->display.setFont(Config::Font::base);

        drawStr::title(menuString);

        /**
         * /////////////////////////////////////////////
         * /////////// Play Controls Left  /////////////
         * /////////////////////////////////////////////
         *
         * These controls are associated with speed and time.
         */
        x = 0;
        h = ceil(64 * speedPercentage / 100);
        ossm->display.drawBox(x, y - h, w, h);
        ossm->display.drawFrame(x, 0, w, 64);
        String speedString = String((int)speedPercentage) + "%";
        ossm->display.setFont(Config::Font::base);
        ossm->display.drawUTF8(x + w + padding, lh1,
                               UserConfig::language.Speed.c_str());
        ossm->display.drawUTF8(x + w + padding, lh2, speedString.c_str());
        ossm->display.setFont(Config::Font::small);
        ossm->display.drawUTF8(
            x + w + padding, lh3,
            formatTime(currentTime - ossm->sessionStartTime).c_str());

        /**
         * /////////////////////////////////////////////
         * /////////// Play Controls Right  ////////////
         * /////////////////////////////////////////////
         *
         * These controls are associated with stroke and distance
         */
        x = 124;
        h = ceil(64 * ossm->encoder.readEncoder() / 100);
        ossm->display.drawBox(x - w, y - h, w, h);
        ossm->display.drawFrame(x - w, 0, w, 64);

        // The word "stroke"
        ossm->display.setFont(Config::Font::base);

        String strokeString = UserConfig::language.Stroke;
        auto stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(x - w - stringWidth - padding, lh1,
                               strokeString.c_str());

        // The stroke percent
        strokeString = String(ossm->encoder.readEncoder()) + "%";
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(x - w - stringWidth - padding, lh2,
                               strokeString.c_str());

        // The stroke count
        ossm->display.setFont(Config::Font::small);
        strokeString = "# " + String(ossm->sessionStrokeCount);
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(x - w - stringWidth - padding, lh3,
                               strokeString.c_str());

        // The Session travel distance
        strokeString = formatDistance(ossm->sessionDistanceMeters);
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(x - w - stringWidth - padding, lh4,
                               strokeString.c_str());

        ossm->display.sendBuffer();

        // Saying hi to the watchdog :).
        vTaskDelay(200);
    }

    // Clean up!
    ossm->encoder.setAcceleration(0);
    ossm->encoder.disableAcceleration();

    vTaskDelete(nullptr);
}

void OSSM::drawPlayControls() {
    xTaskCreatePinnedToCore(drawPlayControlsTask, "drawPlayControlsTask", 2048,
                            this, 1, &displayTask, 0);
}
