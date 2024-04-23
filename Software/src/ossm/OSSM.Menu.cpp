#include "OSSM.h"

#include "constants/Config.h"
#include "constants/Images.h"
#include "extensions/u8g2Extensions.h"
#include "utils/analog.h"

void OSSM::drawMenuTask(void *pvParameters) {
    bool isFirstDraw = true;
    OSSM *ossm = (OSSM *)pvParameters;

    int lastEncoderValue = ossm->encoder.readEncoder();
    int currentEncoderValue;
    int clicksPerRow = 3;
    const int maxClicks = clicksPerRow * (Menu::NUM_OPTIONS)-1;
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
        if (!isFirstDraw && !ossm->encoder.encoderChanged()) {
            vTaskDelay(1);
            continue;
        }

        isFirstDraw = false;
        currentEncoderValue = ossm->encoder.readEncoder();

        ossm->display.clearBuffer();

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
        String menuName = menuStrings[menuOption];
        ESP_LOGD("Menu", "Hovering over state: %s", menuName.c_str());

        // Loop around to make an infinite menu.
        int lastIdx =
            menuOption - 1 < 0 ? Menu::NUM_OPTIONS - 1 : menuOption - 1;
        int nextIdx =
            menuOption + 1 > Menu::NUM_OPTIONS - 1 ? 0 : menuOption + 1;

        ossm->display.setFont(Config::Font::base);

        // Draw the previous item
        if (lastIdx >= 0) {
            ossm->display.drawUTF8(leftPadding, itemHeight * (1),
                                   menuStrings[lastIdx].c_str());
        }

        // Draw the next item
        if (nextIdx < Menu::NUM_OPTIONS) {
            ossm->display.drawUTF8(leftPadding, itemHeight * (3),
                                   menuStrings[nextIdx].c_str());
        }

        // Draw the current item
        ossm->display.setFont(Config::Font::bold);
        ossm->display.drawUTF8(leftPadding, itemHeight * (2), menuName.c_str());

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

        // Display the appropriate Wi-Fi icon based on the current Wi-Fi status
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

        ossm->display.sendBuffer();

        vTaskDelay(1);
    };

    vTaskDelete(nullptr);
}

void OSSM::drawMenu() {
    // start the draw menu task
    xTaskCreatePinnedToCore(drawMenuTask, "drawMenuTask", 2048, this, 1,
                            &displayTask, 0);
}