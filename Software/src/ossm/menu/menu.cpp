#include "menu.h"

#include <WiFi.h>

#include "constants/Config.h"
#include "constants/Images.h"
#include "constants/Menu.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/state/menu.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/tasks.h"
#include "utils/analog.h"

namespace sml = boost::sml;
using namespace sml;

namespace menu {

static void drawMenuTask(void *pvParameters) {
    bool isFirstDraw = true;

    int lastEncoderValue = encoder.readEncoder();
    int currentEncoderValue;
    int clicksPerRow = 3;
    const int maxClicks = clicksPerRow * (Menu::NUM_OPTIONS)-1;
    // Last Wifi State
    wl_status_t wifiState = WL_IDLE_STATUS;

    encoder.setBoundaries(0, maxClicks, true);
    encoder.setAcceleration(0);

    menuState.currentOption =
        (Menu)floor(encoder.readEncoder() / clicksPerRow);

    encoder.setAcceleration(0);

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("menu"_s) || stateMachine->is("menu.idle"_s);
    };

    while (isInCorrectState()) {
        wl_status_t newWifiState = WiFiClass::status();

        // Force redraw on first draw or when conditions change
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
            clearPage(true,
                      false);  // Clear page content but preserve header icons

            // Drawing Variables.
            int leftPadding = 6;  // Padding on the left side of the screen
            int fontSize = 8;
            int itemHeight = 20;   // Height of each item
            int visibleItems = 3;  // Number of items visible on the screen

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

            drawShape::scroll(100 * encoder.readEncoder() /
                              (clicksPerRow * Menu::NUM_OPTIONS - 1));
            const char *menuName = menuStrings[currentOption];
            ESP_LOGD("Menu", "Hovering over state: %s", menuName);

            // Loop around to make an infinite menu.
            int lastIdx =
                currentOption - 1 < 0 ? Menu::NUM_OPTIONS - 1 : currentOption - 1;
            int nextIdx =
                currentOption + 1 > Menu::NUM_OPTIONS - 1 ? 0 : currentOption + 1;

            display.setFont(Config::Font::base);

            // Draw the previous item
            if (lastIdx >= 0) {
                display.drawUTF8(leftPadding, itemHeight * (1),
                                 menuStrings[lastIdx]);
            }

            // Draw the next item
            if (nextIdx < Menu::NUM_OPTIONS) {
                display.drawUTF8(leftPadding, itemHeight * (3),
                                 menuStrings[nextIdx]);
            }

            // Draw the current item
            display.setFont(Config::Font::bold);
            display.drawUTF8(leftPadding, itemHeight * (2), menuName);

            // Draw a rounded rectangle around the center item
            display.drawRFrame(
                0,
                itemHeight * (visibleItems / 2) - (fontSize - itemHeight) / 2,
                120, itemHeight, 2);

            // Draw Shadow.
            display.drawLine(2, 2 + fontSize / 2 + 2 * itemHeight, 119,
                             2 + fontSize / 2 + 2 * itemHeight);
            display.drawLine(120, 4 + fontSize / 2 + itemHeight, 120,
                             1 + fontSize / 2 + 2 * itemHeight);

            refreshPage(true,
                        true);  // Include both footer and header in refresh
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(1);
    };

    // Clear header icons when exiting menu
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearIcons();
        refreshIcons();
        xSemaphoreGive(displayMutex);
    }

    vTaskDelete(nullptr);
}

void drawMenu() {
    int stackSize = 5 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawMenuTask, "drawMenuTask", stackSize, nullptr, 1,
                &Tasks::drawMenuTaskH);
}

}  // namespace menu
