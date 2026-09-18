#include "update.h"

#include "components/HeaderBar.h"
#include "ossm/state/network.h"
#include "services/display.h"
#include "ui.h"

namespace pages {

void drawUpdate() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::updateCheckingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawNoUpdate() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::noUpdatePage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawUpdating() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::drawTextPage(display.getU8g2(), ui::pages::updatingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

const char* networkErrorReason(const String& code) {
    if (code == "wifi") return ui::strings::reasonWifi;
    if (code == "low-memory") return ui::strings::reasonLowMemory;
    if (code == "check-failed") return ui::strings::reasonCheckFailed;
    if (code == "install-failed") return ui::strings::reasonInstallFailed;
    if (code == "pairing-failed") return ui::strings::reasonPairingFailed;
    return ui::strings::reasonUnknown;
}

void drawUpdateAvailable() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::TextPage page = ui::pages::updateAvailablePage;
        page.subtitle = networkStatus.targetVersion.c_str();
        ui::drawTextPage(display.getU8g2(), page);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

void drawUpdateFailed() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        ui::TextPage page = ui::pages::updateFailedPage;
        page.subtitle = networkErrorReason(networkStatus.error);
        ui::drawTextPage(display.getU8g2(), page);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }
}

}  // namespace pages
