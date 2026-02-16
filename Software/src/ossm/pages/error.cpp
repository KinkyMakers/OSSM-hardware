#include "error.h"

#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/state/error.h"
#include "services/display.h"
#include "services/stepper.h"

namespace pages {

void drawError() {
    // Throw the e-break on the stepper
    try {
        stepper->forceStop();
    } catch (const std::exception &e) {
        ESP_LOGD("pages::drawError", "Caught exception: %s", e.what());
    }

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(UserConfig::language.Error);
        drawStr::multiLine(0, 20, errorState.message);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
