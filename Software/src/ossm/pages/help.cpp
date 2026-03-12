#include "help.h"

#include "components/HeaderBar.h"
#include "services/display.h"
#include "ui.h"

namespace pages {

void drawHelp() {
    showHeaderIcons = false;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::helpPage);
        display.sendBuffer();
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
