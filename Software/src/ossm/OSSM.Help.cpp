
#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "qrcode.h"

void OSSM::drawHelp() {
    if (xSemaphoreTake(displayMutex, 100) == pdTRUE) {
        clearPage(true, true);

        static QRCode qrcode;
        const int scale = 2;
        const int version = 3;
        // This Version of QR Codes can handle ~61 alphanumeric characters with
        // ECC LEVEL M

        // NOLINTBEGIN(modernize-avoid-c-arrays)
        uint8_t qrcodeData[qrcode_getBufferSize(3)];
        // NOLINTEND(modernize-avoid-c-arrays)

        const char url[] PROGMEM = "https://unbox.ossm.tech";

        qrcode_initText(&qrcode, qrcodeData, 3, 0, url);

        int yOffset = constrain((64 - qrcode.size * scale) / 2, 0, 64);
        int xOffset = constrain((128 - qrcode.size * scale), 0, 128);

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
    display.drawUTF8(0, 10, UserConfig::language.GetHelp.c_str());
    // Draw line
    display.drawHLine(0, 12, xOffset - 10);
    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, UserConfig::language.GetHelpLine1.c_str());
    display.drawUTF8(0, 38, UserConfig::language.GetHelpLine2.c_str());
    display.drawUTF8(0, 62, UserConfig::language.Skip.c_str());
    display.sendBuffer();
    displayMutex.unlock();
}
