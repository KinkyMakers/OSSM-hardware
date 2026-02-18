#include "update.h"

#include "Arduino.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "services/display.h"

namespace pages {

void drawUpdate() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(F("Checking for update..."));

        // TODO - Add a spinner here
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawNoUpdate() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(F("No Update Available"));
        display.drawUTF8(0, 62, UserConfig::language.Skip);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawUpdating() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(F("Updating OSSM..."));
        drawStr::multiLine(0, 24, UserConfig::language.UpdateMessage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
