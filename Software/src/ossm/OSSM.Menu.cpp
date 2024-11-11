#include "OSSM.h"

#include "constants/Config.h"
#include "constants/Images.h"
#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"

void OSSM::drawMenuTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;

    int numberOfSelection = Menu::NUM_OPTIONS;
    int nextSelection = (int)ossm->menuOption;
    int encoderClicksPerRow = 3;

    // Init encoder
    ossm->encoder.setAcceleration(10);
    ossm->encoder.setBoundaries(0, numberOfSelection * encoderClicksPerRow - 1, true);
    ossm->encoder.setEncoderValue(nextSelection * encoderClicksPerRow);

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("menu"_s) ||
               ossm->sm->is("menu.idle"_s);
    };

    bool shouldUpdateDisplay = true;
    while (isInCorrectState(ossm)) {

        // Update selection when encoder moved
        nextSelection = (int)(ossm->encoder.readEncoder() / encoderClicksPerRow);
        shouldUpdateDisplay = shouldUpdateDisplay || (int)ossm->menuOption != nextSelection;

        if (!shouldUpdateDisplay) {
            vTaskDelay(100);
            continue;
        }
        shouldUpdateDisplay = false;

        // Update selected option in current menu
        ossm->menuOption = (Menu)nextSelection;

        // Loop around to make an infinite menu.
        int lastIdx = ossm->menuOption == 0 ? numberOfSelection - 1 : ossm->menuOption - 1;
        int nextIdx = ossm->menuOption + 1 > numberOfSelection - 1 ? 0 : ossm->menuOption + 1;
        ESP_LOGD("Menu", "lastIdx: %d, ossm->menuOption: %d, nextIdx: %d, numberOfSelection: %d",
                  lastIdx, ossm->menuOption, nextIdx, numberOfSelection);
        drawMenuOnDisplay(ossm, menuStrings, lastIdx, ossm->menuOption, nextIdx, numberOfSelection);

        vTaskDelay(1);
    };

    vTaskDelete(nullptr);
}

void OSSM::drawMenuOnDisplay(OSSM *ossm, String *strings,
                             int lastIdx, int idx, int nextIdx, int numberIdx) {

    displayMutex.lock();
    ossm->display.clearBuffer();

    // Drawing Variables.
    int leftPadding = 6;  // Padding on the left side of the screen
    int fontSize = 8;
    int itemHeight = 20;   // Height of each item
    int visibleItems = 3;  // Number of items visible on the screen

    ossm->display.setFont(Config::Font::base);

    // Draw the previous item
    if (lastIdx >= 0) {
        ossm->display.drawUTF8(leftPadding, itemHeight * (1),
                                strings[lastIdx].c_str());
    }

    // Draw the next item
    if (nextIdx < numberIdx) {
        ossm->display.drawUTF8(leftPadding, itemHeight * (3),
                                strings[nextIdx].c_str());
    }

    // Draw the current item
    ossm->display.setFont(Config::Font::bold);
    ossm->display.drawUTF8(leftPadding, itemHeight * (2), strings[idx].c_str());

    // Draw a rounded rectangle around the center item
    ossm->display.drawRFrame(
        0, itemHeight * (visibleItems / 2) - (fontSize - itemHeight) / 2,
        120, itemHeight, 2);

    // Draw Shadow.
    ossm->display.drawLine(2, 2 + fontSize / 2 + 2 * itemHeight, 119,
                            2 + fontSize / 2 + 2 * itemHeight);
    ossm->display.drawLine(120, 4 + fontSize / 2 + itemHeight, 120,
                            1 + fontSize / 2 + 2 * itemHeight);

    // Draw the wifi icon
    switch (WiFiClass::status()) {
        case WL_CONNECTED:
            ossm->display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                    WifiIcon::h, WifiIcon::Connected);
            break;
        case WL_NO_SSID_AVAIL:
        case WL_CONNECT_FAILED:
        case WL_DISCONNECTED:
            ossm->display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                    WifiIcon::h, WifiIcon::Error);
            break;
        case WL_IDLE_STATUS:
            ossm->display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                    WifiIcon::h, WifiIcon::First);
            break;
        default:
            ossm->display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                    WifiIcon::h, WifiIcon::Error);
            break;
    }

    // Drow scroll bar
    drawShape::scroll(100 * idx / (numberIdx - 1));

    ossm->display.sendBuffer();
    displayMutex.unlock();
}

void OSSM::drawMenu() {
    int stackSize = 5 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawMenuTask, "drawMenuTask", stackSize, this, 1,
                &drawMenuTaskH);
}
