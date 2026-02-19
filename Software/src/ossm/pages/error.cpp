#include "error.h"

#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/state/error.h"
#include "services/display.h"
#include "services/stepper.h"

namespace pages {

void drawError() {
    stepper->forceStop();

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);
        drawStr::title(UserConfig::language.Error);
        drawStr::multiLine(0, 20, errorState.message);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
