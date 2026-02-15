#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"
#include "utils/format.h"

void OSSM::drawPreflightTask(void *pvParameters) {
    // parse ossm from the parameters
    OSSM *ossm = (OSSM *)pvParameters;
    auto menuString = menuStrings[ossm->menuOption];
    float speedPercentage;

    // Set the stepper to the home position
    ossm->stepper->setAcceleration(1000_mm);
    ossm->stepper->setSpeedInHz(25_mm);
    ossm->stepper->moveTo(0, false);

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
               ossm->sm->is("streaming.preflight"_s) ||
               ossm->sm->is("strokeEngine.preflight"_s);
    };

    do {
#ifdef AJ_DEVELOPMENT_HARDWARE
        speedPercentage = 0;
#else
        speedPercentage =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
#endif
        if (speedPercentage < Config::Advanced::commandDeadZonePercentage) {
            ossm->sm->process_event(Done{});
            break;
        };

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearPage(true, true);
            drawStr::title(menuString);
            String speedString = UserConfig::language.Speed + String(": ") +
                                 String((int)speedPercentage) + "%";
            drawStr::centered(25, speedString);
            drawStr::multiLine(0, 40, UserConfig::language.SpeedWarning);

            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(100);
    } while (isInPreflight(ossm));

    vTaskDelete(nullptr);
};

void OSSM::drawPreflight() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPreflightTask, "drawPlayControlsTask", stackSize, this, 1,
                &Tasks::drawPreflightTaskH);
}
