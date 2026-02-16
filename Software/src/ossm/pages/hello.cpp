#include "hello.h"

#include <array>

#include "constants/Images.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "services/display.h"
#include "services/tasks.h"

namespace pages {

static void drawHelloTask(void *pvParameters) {
    int frameIdx = 0;
    const int nFrames = 8;

    int startX = 24;
    int offsetY = 12;

    // Bounce the Y position from 0 to 32, up to 24 and down to 32
    std::array<int, 8> framesY = {6, 12, 24, 48, 44, 42, 44, 48};
    std::array<int, 4> heights = {0, 0, 0, 0};
    int letterSpacing = 20;

    while (frameIdx < nFrames + 9) {
        if (frameIdx < nFrames) {
            heights[0] = framesY[frameIdx] - offsetY;
        }
        if (frameIdx - 3 > 0 && frameIdx - 3 < nFrames) {
            heights[1] = framesY[frameIdx - 3] - offsetY;
        }
        if (frameIdx - 6 > 0 && frameIdx - 6 < nFrames) {
            heights[2] = framesY[frameIdx - 6] - offsetY;
        }
        if (frameIdx - 9 > 0 && frameIdx - 9 < nFrames) {
            heights[3] = framesY[frameIdx - 9] - offsetY;
        }
        // increment the frame index
        frameIdx++;

        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            clearPage(true, true);
            display.setFont(u8g2_font_maniac_tf);
            display.drawUTF8(startX, heights[0], "O");
            display.drawUTF8(startX + letterSpacing, heights[1], "S");
            display.drawUTF8(startX + letterSpacing * 2, heights[2], "S");
            display.drawUTF8(startX + letterSpacing * 3, heights[3], "M");
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }
        // Saying hi to the watchdog :).
        vTaskDelay(1);
    };

    // Delay for a second, then show the RDLogo.
    vTaskDelay(1500);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(
            "Research & Desire         ");  // Padding to offset from BLE icons
        display.drawXBMP(35, 14, 57, 50, Images::RDLogo);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    vTaskDelay(1000);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(
            "Kinky Makers       ");  // Padding to offset from BLE icons
        display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    vTaskDelay(1000);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        std::string measuringStrokeTitle =
            std::string(UserConfig::language.MeasuringStroke) +
            "         ";  // Padding to offset from BLE icons
        drawStr::title(measuringStrokeTitle.c_str());
        display.drawXBMP(40, 14, 50, 50, Images::KMLogo);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    // delete the task
    vTaskDelete(nullptr);
}

void drawHello() {
    // 3 x minimum stack
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawHelloTask, "drawHello", stackSize, nullptr, 1,
                &Tasks::drawHelloTaskH);
}

}  // namespace pages
