
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
        return ossm->sm->is("simplePenetration.preflight"_s) ||
               ossm->sm->is("strokeEngine.preflight"_s);
    };

    do {
        speedPercentage =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        if (speedPercentage < Config::Advanced::commandDeadZonePercentage) {
            ossm->sm->process_event(Done{});
            break;
        };

        displayMutex.lock();
        ossm->display.clearBuffer();
        drawStr::title(menuString);
        String speedString = UserConfig::language.Speed + ": " +
                             String((int)speedPercentage) + "%";
        drawStr::centered(25, speedString);
        drawStr::multiLine(0, 40, UserConfig::language.SpeedWarning);

        ossm->display.sendBuffer();
        displayMutex.unlock();

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
               ossm->sm->is("simplePenetration.idle"_s) ||
               ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s);
    };

    // Prepare the encoder
    ossm->encoder.setBoundaries(0, 100, false);
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setEncoderValue(0);

    // TODO: prepare the stepper with safe values.

    int16_t w = 10;
    int16_t x;
    int16_t padding = 4;

    // Line heights
    short lh3 = 56;
    short lh4 = 64;
    static float speedKnobPercent = 0;

    // record session start time rounded to the nearest second
    ossm->sessionStartTime = floor(millis() / 1000);
    ossm->sessionStrokeCount = 0;
    ossm->sessionDistanceMeters = 0;

    bool valueChanged = false;

    bool isStrokeEngine =
        ossm->sm->is("strokeEngine"_s) || ossm->sm->is("strokeEngine.idle"_s);

    while (isInCorrectState(ossm)) {
        speedKnobPercent =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        speedPercentage = 0.3 * speedKnobPercent + 0.7 * speedPercentage;
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

        String strokeString = UserConfig::language.Stroke;
        auto stringWidth = ossm->display.getUTF8Width(strokeString.c_str());

        displayMutex.lock();
        ossm->display.clearBuffer();
        ossm->display.setFont(Config::Font::base);

        drawShape::settingBar(UserConfig::language.Speed, speedKnobPercent);

        if (isStrokeEngine) {
            switch (ossm->strokeEngineControl) {
                case StrokeEngineControl::STROKE:
                    drawShape::settingBarSmall(speedKnobPercent, 125);
                    drawShape::settingBarSmall(speedKnobPercent, 120);
                    drawShape::settingBar(strokeString,
                                          ossm->encoder.readEncoder(), 118, 0,
                                          RIGHT_ALIGNED);
                    break;
                case StrokeEngineControl::SENSATION:
                    drawShape::settingBarSmall(speedKnobPercent, 125);
                    drawShape::settingBar(strokeString,
                                          ossm->encoder.readEncoder(), 123, 0,
                                          RIGHT_ALIGNED, 5);
                    drawShape::settingBarSmall(speedKnobPercent, 108);

                    break;
                case StrokeEngineControl::DEPTH:
                    drawShape::settingBar(strokeString,
                                          ossm->encoder.readEncoder(), 128, 0,
                                          RIGHT_ALIGNED, 10);
                    drawShape::settingBarSmall(speedKnobPercent, 113);
                    drawShape::settingBarSmall(speedKnobPercent, 108);

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
        strokeString = formatTime(currentTime - ossm->sessionStartTime).c_str();
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(104 - stringWidth, lh3, strokeString.c_str());

        strokeString = formatDistance(ossm->sessionDistanceMeters);
        stringWidth = ossm->display.getUTF8Width(strokeString.c_str());
        ossm->display.drawUTF8(104 - stringWidth, lh4, strokeString.c_str());

        ossm->display.sendBuffer();
        displayMutex.unlock();

        vTaskDelay(200);
    }

    // Clean up!
    ossm->encoder.setAcceleration(0);
    ossm->encoder.disableAcceleration();

    vTaskDelete(nullptr);
};

void OSSM::drawPreflightTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    auto menuString = menuStrings[ossm->menuOption];
    float speedPercentage;

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
        return ossm->sm->is("simplePenetration.preflight"_s) ||
               ossm->sm->is("strokeEngine.preflight"_s);
    };

    do {
        speedPercentage =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
        if (speedPercentage < Config::Advanced::commandDeadZonePercentage) {
            ossm->sm->process_event(Done{});
            break;
        };

        displayMutex.lock();
        ossm->display.clearBuffer();
        drawStr::title(menuString);
        String speedString = UserConfig::language.Speed + ": " +
                             String((int)speedPercentage) + "%";
        drawStr::centered(25, speedString);
        drawStr::multiLine(0, 40, UserConfig::language.SpeedWarning);

        ossm->display.sendBuffer();
        displayMutex.unlock();

        vTaskDelay(100);
    } while (isInPreflight(ossm));

    vTaskDelete(nullptr);
};

void OSSM::drawPlayControls() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPlayControlsTask, "drawPlayControlsTask", stackSize, this,
                1, &drawPlayControlsTaskH);
}

void OSSM::drawPreflight() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPreflightTask, "drawPlayControlsTask", stackSize, this, 1,
                &drawPreflightTaskH);
}
