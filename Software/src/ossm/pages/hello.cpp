#include "hello.h"

#include <string>

#include "HelloAnimation.h"
#include "Logos.h"
#include "Strings.h"
#include "components/HeaderBar.h"
#include "constants/Version.h"
#include "services/display.h"
#include "services/tasks.h"
#include "ui.h"

namespace pages {

static void drawHelloTask(void *pvParameters) {
    showHeaderIcons = false;

    for (int i = 0; i < ui::HELLO_FRAME_COUNT; i++) {
        if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
            ui::drawHelloFrame(display.getU8g2(), ui::HELLO_FRAMES[i],
                               VERSION);
            refreshPage(true, true);
            xSemaphoreGive(displayMutex);
        }
        vTaskDelay(1);
    }

    vTaskDelay(1500);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::LogoData rdLogo{ui::strings::researchAndDesire,
                            ui::logos::RDLogo, 57, 50, 35, 14, VERSION};
        ui::drawLogo(display.getU8g2(), rdLogo);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    vTaskDelay(1000);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::LogoData kmLogo{ui::strings::kinkyMakers, ui::logos::KMLogo,
                            50, 50, 40, 14, VERSION};
        ui::drawLogo(display.getU8g2(), kmLogo);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    vTaskDelay(1000);

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        std::string measuringTitle =
            std::string(ui::strings::measuringStroke) + "         ";
        ui::LogoData measuring{measuringTitle.c_str(), ui::logos::KMLogo,
                               50, 50, 40, 14, VERSION};
        ui::drawLogo(display.getU8g2(), measuring);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    vTaskDelete(nullptr);
}

void drawHello() {
    int stackSize = 3 * configMINIMAL_STACK_SIZE;
    xTaskCreate(drawHelloTask, "drawHello", stackSize, nullptr, 1,
                &Tasks::drawHelloTaskH);
}

}  // namespace pages
