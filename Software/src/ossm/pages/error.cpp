#include "error.h"

#include "ossm/state/error.h"
#include "services/display.h"
#include "services/stepper.h"
#include "ui.h"

namespace pages {

void drawError() {
    stepper->forceStop();

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::TextPage page = ui::pages::errorPage;
        page.body = errorState.message.c_str();
        ui::drawTextPage(display.getU8g2(), page);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
