#include "wifi.h"

#include <WiFi.h>

#include "constants/Config.h"
#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "qrcode.h"
#include "services/display.h"
#include "services/wm.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

void drawWiFi() {
    if (xSemaphoreTake(displayMutex, 200) == pdTRUE) {
        clearPage(true, true);

        bool isConnected = WiFiClass::status() == WL_CONNECTED;

        if (isConnected) {
            display.setFont(Config::Font::bold);
            display.drawUTF8(0, 10, UserConfig::language.WiFiSetup);
            display.drawHLine(0, 12, 118);

            display.setFont(Config::Font::base);
            display.drawUTF8(0, 28, "Connected to:");
            display.drawUTF8(0, 40, WiFi.SSID().c_str());
            display.drawUTF8(0, 52, WiFi.localIP().toString().c_str());
            display.drawUTF8(0, 62, "Long press to reset WiFi");
        } else {
            static QRCode qrcode;
            const int scale = 2;

            // NOLINTBEGIN(modernize-avoid-c-arrays)
            uint8_t qrcodeData[qrcode_getBufferSize(3)];
            // NOLINTEND(modernize-avoid-c-arrays)

            const char url[] PROGMEM = "WIFI:S:OSSM Setup;T:nopass;;";
            qrcode_initText(&qrcode, qrcodeData, 3, 0, url);

            int yOffset = 64 - qrcode.size * scale;   // Bottom align
            int xOffset = 128 - qrcode.size * scale;  // Right align

            for (uint8_t y = 0; y < qrcode.size; y++) {
                for (uint8_t x = 0; x < qrcode.size; x++) {
                    if (qrcode_getModule(&qrcode, x, y)) {
                        display.drawBox(xOffset + x * scale,
                                        yOffset + y * scale, scale, scale);
                    }
                }
            }

            display.setFont(Config::Font::bold);
            display.drawUTF8(0, 10, UserConfig::language.WiFiSetup);
            display.drawHLine(0, 12, xOffset - 10);

            display.setFont(Config::Font::base);
            display.drawUTF8(0, 26, UserConfig::language.WiFiSetupLine1);
            display.drawUTF8(0, 38, UserConfig::language.WiFiSetupLine2);
            display.drawUTF8(0, 62, UserConfig::language.Restart);
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
        wm.startConfigPortal("OSSM Setup");

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
