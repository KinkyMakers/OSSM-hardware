#include "OSSM.h"

#include "constants/Config.h"
#include "constants/Images.h"
#include "extensions/u8g2Extensions.h"
#include "services/encoder.h"
#include "state/globalstate.h"
#include "utils/analog.h"

void OSSM::drawMenuTask(void *pvParameters) {
    bool isFirstDraw = true;
    OSSM *ossm = (OSSM *)pvParameters;

    int lastEncoderValue = encoder.readEncoder();
    int currentEncoderValue;
    int clicksPerRow = 3;
    const int maxClicks = clicksPerRow * (Menu::NUM_OPTIONS)-1;
    encoder.setBoundaries(0, maxClicks, true);
    encoder.setAcceleration(0);

    ossm->menuOption = (Menu)floor(encoder.readEncoder() / clicksPerRow);

    encoder.setAcceleration(0);
    //    encoder.setEncoderValue(0);

    // get the encoder position

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return stateMachine.is("menu"_s) || stateMachine.is("menu.idle"_s);
    };

    while (isInCorrectState(ossm)) {
        if (!isFirstDraw && !encoder.encoderChanged()) {
            vTaskDelay(1);
            continue;
        }

        isFirstDraw = false;
        currentEncoderValue = encoder.readEncoder();

        display.clearBuffer();

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

        drawShape::scroll(100 * encoder.readEncoder() /
                          (clicksPerRow * Menu::NUM_OPTIONS - 1));
        String menuName = menuStrings[menuOption];
        ESP_LOGD("Menu", "Hovering over state: %s", menuName.c_str());

        // Loop around to make an infinite menu.
        int lastIdx =
            menuOption - 1 < 0 ? Menu::NUM_OPTIONS - 1 : menuOption - 1;
        int nextIdx =
            menuOption + 1 > Menu::NUM_OPTIONS - 1 ? 0 : menuOption + 1;

        display.setFont(Config::Font::base);

        // Draw the previous item
        if (lastIdx >= 0) {
            display.drawUTF8(leftPadding, itemHeight * (1),
                             menuStrings[lastIdx].c_str());
        }

        // Draw the next item
        if (nextIdx < Menu::NUM_OPTIONS) {
            display.drawUTF8(leftPadding, itemHeight * (3),
                             menuStrings[nextIdx].c_str());
        }

        // Draw the current item
        display.setFont(Config::Font::bold);
        display.drawUTF8(leftPadding, itemHeight * (2), menuName.c_str());

        // Draw a rounded rectangle around the center item
        display.drawRFrame(
            0, itemHeight * (visibleItems / 2) - (fontSize - itemHeight) / 2,
            120, itemHeight, 2);

        // Draw Shadow.
        display.drawLine(2, 2 + fontSize / 2 + 2 * itemHeight, 119,
                         2 + fontSize / 2 + 2 * itemHeight);
        display.drawLine(120, 4 + fontSize / 2 + itemHeight, 120,
                         1 + fontSize / 2 + 2 * itemHeight);

        // Draw the wifi icon

        // Display the appropriate Wi-Fi icon based on the current Wi-Fi status
        switch (WiFiClass::status()) {
            case WL_CONNECTED:
                display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                 WifiIcon::h, WifiIcon::Connected);
                break;
            case WL_NO_SSID_AVAIL:
            case WL_CONNECT_FAILED:
            case WL_DISCONNECTED:
                display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                 WifiIcon::h, WifiIcon::Error);
                break;
            case WL_IDLE_STATUS:
                display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                 WifiIcon::h, WifiIcon::First);
                break;
            default:
                display.drawXBMP(WifiIcon::x, WifiIcon::y, WifiIcon::w,
                                 WifiIcon::h, WifiIcon::Error);
                break;
        }

        display.sendBuffer();

        vTaskDelay(1);
    };

    vTaskDelete(nullptr);
}

void OSSM::drawMenu() {
    // start the draw menu task
    xTaskCreatePinnedToCore(drawMenuTask, "drawMenuTask", 2048, this, 1,
                            &displayTask, 0);
}