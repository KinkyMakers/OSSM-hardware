
#include "OSSM.h"

#include <services/wm.h>

#include "extensions/u8g2Extensions.h"
#include "qrcode.h"

void OSSM::drawWiFi() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);

        static QRCode qrcode;
        const int scale = 2;
        // This Version of QR Codes can handle ~61 alphanumeric characters with
        // ECC LEVEL M

        // NOLINTBEGIN(modernize-avoid-c-arrays)
        uint8_t qrcodeData[qrcode_getBufferSize(3)];
        // NOLINTEND(modernize-avoid-c-arrays)

        const char url[] PROGMEM = "WIFI:S:OSSM Setup;T:nopass;;";

        qrcode_initText(&qrcode, qrcodeData, 3, 0, url);

        int yOffset = 64 - qrcode.size * scale;   // Bottom align
        int xOffset = 128 - qrcode.size * scale;  // Right align

        // Draw the QR code
        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    display.drawBox(xOffset + x * scale, yOffset + y * scale,
                                    scale, scale);
                }
            }
        }

        display.setFont(Config::Font::bold);
        display.drawUTF8(0, 10, UserConfig::language.WiFiSetup);
        // Draw line
        display.drawHLine(0, 12, xOffset - 10);

        display.setFont(Config::Font::base);
        display.drawUTF8(0, 26, UserConfig::language.WiFiSetupLine1);
        display.drawUTF8(0, 38, UserConfig::language.WiFiSetupLine2);
        display.drawUTF8(0, 62, UserConfig::language.Restart);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    wm.setConfigPortalBlocking(false);
    wm.setCleanConnect(true);
    wm.startConfigPortal("OSSM Setup");

    // wm process task
    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            OSSM *ossm = (OSSM *)pvParameters;

            auto isInCorrectState = [](OSSM *ossm) {
                // Add any states that you want to support here.
                return ossm->sm->is("wifi"_s) || ossm->sm->is("wifi.idle"_s);
            };

            while (isInCorrectState(ossm)) {
                wm.process();
                vTaskDelay(50);
            }

            wm.stopConfigPortal();
            vTaskDelete(nullptr);
        },
        "wmProcessTask", 4 * configMINIMAL_STACK_SIZE, this,
        configMAX_PRIORITIES - 1, nullptr, 0);
}
