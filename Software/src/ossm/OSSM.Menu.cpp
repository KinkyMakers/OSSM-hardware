#include "OSSM.h"

#include "constants/Config.h"
#include "constants/Images.h"
#include "extensions/u8g2Extensions.h"
#include "services/display.h"
#include "utils/analog.h"

void OSSM::drawMenuTask(void *pvParameters) {
    bool isFirstDraw = true;
    OSSM *ossm = (OSSM *)pvParameters;

    int lastEncoderValue = ossm->encoder.readEncoder();
    int currentEncoderValue;
    int clicksPerRow = 3;
    const int maxClicks = clicksPerRow * (Menu::NUM_OPTIONS)-1;
    // Last Wifi STate
    wl_status_t wifiState = WL_IDLE_STATUS;

    ossm->encoder.setBoundaries(0, maxClicks, true);
    ossm->encoder.setAcceleration(0);

    ossm->menuOption = (Menu)floor(ossm->encoder.readEncoder() / clicksPerRow);

    ossm->encoder.setAcceleration(0);
    //    ossm->encoder.setEncoderValue(0);

    // get the encoder position

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("menu"_s) || ossm->sm->is("menu.idle"_s);
    };

    while (isInCorrectState(ossm)) {
        wl_status_t newWifiState = WiFiClass::status();
        
        // Force redraw on first draw or when conditions change
        bool shouldRedraw = isFirstDraw || ossm->encoder.encoderChanged() || (wifiState != newWifiState);
        
        if (!shouldRedraw) {
            vTaskDelay(50);
            continue;
        }

        wifiState = newWifiState;

        isFirstDraw = false;
        currentEncoderValue = ossm->encoder.readEncoder();

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearPage(true, false); // Clear page content but preserve header icons

            // Drawing Variables.
            int leftPadding = 6;  // Padding on the left side of the screen
            int fontSize = 8;
            int itemHeight = 20;   // Height of each item
            int visibleItems = 3;  // Number of items visible on the screen

            auto menuOption = ossm->menuOption;
            if (abs(currentEncoderValue % maxClicks -
                    lastEncoderValue % maxClicks) >= clicksPerRow) {
                lastEncoderValue = currentEncoderValue % maxClicks;
                menuOption = (Menu)floor(lastEncoderValue / clicksPerRow);

                ossm->menuOption = menuOption;
            }

            ESP_LOGD(
                "Menu",
                "currentEncoderValue: %d, lastEncoderValue: %d, menuOption: %d",
                currentEncoderValue, lastEncoderValue, menuOption);

            drawShape::scroll(100 * ossm->encoder.readEncoder() /
                              (clicksPerRow * Menu::NUM_OPTIONS - 1));
            const char *menuName = menuStrings[menuOption];
            ESP_LOGD("Menu", "Hovering over state: %s", menuName);

            // Loop around to make an infinite menu.
            int lastIdx =
                menuOption - 1 < 0 ? Menu::NUM_OPTIONS - 1 : menuOption - 1;
            int nextIdx =
                menuOption + 1 > Menu::NUM_OPTIONS - 1 ? 0 : menuOption + 1;

            ossm->display.setFont(Config::Font::base);

            // Draw the previous item
            if (lastIdx >= 0) {
                ossm->display.drawUTF8(leftPadding, itemHeight * (1),
                                       menuStrings[lastIdx]);
            }

            // Draw the next item
            if (nextIdx < Menu::NUM_OPTIONS) {
                ossm->display.drawUTF8(leftPadding, itemHeight * (3),
                                       menuStrings[nextIdx]);
            }

            // Draw the current item
            ossm->display.setFont(Config::Font::bold);
            ossm->display.drawUTF8(leftPadding, itemHeight * (2), menuName);

            // Draw a rounded rectangle around the center item
            ossm->display.drawRFrame(
                0,
                itemHeight * (visibleItems / 2) - (fontSize - itemHeight) / 2,
                120, itemHeight, 2);

            // Draw Shadow.
            ossm->display.drawLine(2, 2 + fontSize / 2 + 2 * itemHeight, 119,
                                   2 + fontSize / 2 + 2 * itemHeight);
            ossm->display.drawLine(120, 4 + fontSize / 2 + itemHeight, 120,
                                   1 + fontSize / 2 + 2 * itemHeight);

            refreshPage(true, true); // Include both footer and header in refresh
            xSemaphoreGive(displayMutex);
        }

        vTaskDelay(1);
    };

    // Clear header icons when exiting menu
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearIcons(); // Clear the header icons
        ossm->display.setMaxClipWindow(); // Reset clipping 
        ossm->display.sendBuffer(); // Send the cleared buffer to display
        xSemaphoreGive(displayMutex);
    }

    vTaskDelete(nullptr);
}

void OSSM::drawMenu() {
    int stackSize = 5 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawMenuTask, "drawMenuTask", stackSize, this, 1,
                &Tasks::drawMenuTaskH);
}
