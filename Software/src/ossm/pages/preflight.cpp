#include "preflight.h"

#include "constants/Config.h"
#include "constants/Menu.h"
#include "constants/Pins.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/Events.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "utils/analog.h"
#include "utils/format.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

static void drawPreflightTask(void *pvParameters) {
    auto menuString = menuStrings[menuState.currentOption];
    float speedPercentage;

    /**
     * /////////////////////////////////////////////
     * //// Safely Block High Speeds on Startup ///
     * /////////////////////////////////////////////
     *
     * This is a safety feature to prevent the user from accidentally beginning
     * a session at max speed. After the user decreases the speed to 0.5% or
     * less, the state machine will be allowed to continue.
     */

    auto isInPreflight = []() {
        // Add your preflight checks states here.
        return stateMachine->is("simplePenetration.preflight"_s) ||
               stateMachine->is("strokeEngine.preflight"_s);
    };

    do {
#ifdef AJ_DEVELOPMENT_HARDWARE
        speedPercentage = 0;
#else
        speedPercentage =
            getAnalogAveragePercent(SampleOnPin{Pins::Remote::speedPotPin, 50});
#endif
        if (speedPercentage < Config::Advanced::commandDeadZonePercentage) {
            stateMachine->process_event(Done{});
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
    } while (isInPreflight());

    vTaskDelete(nullptr);
};

void drawPreflight() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawPreflightTask, "drawPreflightTask", stackSize, nullptr, 1,
                &Tasks::drawPreflightTaskH);
}

}  // namespace pages
