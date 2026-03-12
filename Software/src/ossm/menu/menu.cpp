#include "menu.h"

#include <WiFi.h>

#include "constants/Menu.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/tasks.h"
#include "ui.h"
#include "utils/analog.h"
#include "components/HeaderBar.h"

namespace sml = boost::sml;
using namespace sml;

namespace menu {

static void drawMenuTask(void *pvParameters) {
    bool isFirstDraw = true;

    int lastEncoderValue = encoder.readEncoder();
    int currentEncoderValue;
    int clicksPerRow = 3;
    const int maxClicks = clicksPerRow * (Menu::NUM_OPTIONS)-1;
    wl_status_t wifiState = WL_IDLE_STATUS;

    encoder.setBoundaries(0, maxClicks, true);
    encoder.setAcceleration(0);

    menuState.currentOption =
        (Menu)floor(encoder.readEncoder() / clicksPerRow);

    encoder.setAcceleration(0);

    showHeaderIcons = true;

    auto isInCorrectState = []() {
        return stateMachine->is("menu"_s) || stateMachine->is("menu.idle"_s);
    };

    while (isInCorrectState()) {
        wl_status_t newWifiState = WiFiClass::status();

        bool shouldRedraw = isFirstDraw || encoder.encoderChanged() ||
                            (wifiState != newWifiState);

        if (!shouldRedraw) {
            vTaskDelay(50);
            continue;
        }

        wifiState = newWifiState;
        isFirstDraw = false;
        currentEncoderValue = encoder.readEncoder();

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            auto currentOption = menuState.currentOption;
            if (abs(currentEncoderValue % maxClicks -
                    lastEncoderValue % maxClicks) >= clicksPerRow) {
                lastEncoderValue = currentEncoderValue % maxClicks;
                currentOption = (Menu)floor(lastEncoderValue / clicksPerRow);
                menuState.currentOption = currentOption;
            }

            ESP_LOGD("Menu",
                     "currentEncoderValue: %d, lastEncoderValue: %d, "
                     "menuOption: %d",
                     currentEncoderValue, lastEncoderValue, currentOption);

            ui::MenuData data{};
            data.items = menuStrings;
            data.numItems = Menu::NUM_OPTIONS;
            data.selectedIndex = currentOption;

            ui::drawMenu(display.getU8g2(), data);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(1);
    };

    vTaskDelete(nullptr);
}

void drawMenu() {
    int stackSize = 5 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawMenuTask, "drawMenuTask", stackSize, nullptr, 1,
                &Tasks::drawMenuTaskH);
}

}  // namespace menu
