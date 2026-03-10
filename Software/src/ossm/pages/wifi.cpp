#include "wifi.h"

#include <WiFi.h>

#include "Strings.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "services/wm.h"
#include "ui.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

void drawWiFi() {
    if (xSemaphoreTake(displayMutex, 200) == pdTRUE) {
        bool isConnected = WiFiClass::status() == WL_CONNECTED;

        if (!isConnected) {
            ui::drawTextPage(display.getU8g2(),
                             ui::pages::wifiDisconnectedPage);
        } else {
            ui::drawTextPage(display.getU8g2(),
                             ui::pages::wifiConnectedPage);
        }
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    // Only start portal if not already connected
    if (WiFiClass::status() != WL_CONNECTED) {
        wm.setConfigPortalBlocking(false);
        wm.setCleanConnect(true);
        wm.setConnectTimeout(30);
        wm.setConnectRetries(5);
        wm.startConfigPortal(ui::strings::ossmSetup);

        // wm process task
        xTaskCreatePinnedToCore(
            [](void *pvParameters) {
                auto isInCorrectState = []() {
                    return stateMachine->is("wifi"_s) ||
                           stateMachine->is("wifi.idle"_s);
                };

                while (isInCorrectState()) {
                    wm.process();

                    if (WiFiClass::status() == WL_CONNECTED) {
                        stateMachine->process_event(Done{});
                        break;
                    }

                    vTaskDelay(50);
                }

                // Only stop portal if user left the screen, not on successful
                // connection
                if (WiFiClass::status() != WL_CONNECTED) {
                    wm.stopConfigPortal();
                }
                vTaskDelete(nullptr);
            },
            "wmProcessTask", 4 * configMINIMAL_STACK_SIZE, nullptr,
            configMAX_PRIORITIES - 1, nullptr, 0);
    }
}

}  // namespace pages
