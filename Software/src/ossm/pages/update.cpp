#include "update.h"

#include "services/display.h"
#include "ui.h"

namespace pages {

void drawUpdate() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::updateCheckingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawNoUpdate() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::noUpdatePage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawUpdating() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::updatingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
