
#include "OSSM.h"

#include "extensions/u8g2Extensions.h"

void OSSM::drawHelp() {
    displayMutex.lock();
    display.clearBuffer();

    drawShape::drawQRCode("https://unbox.ossm.tech");

    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, UserConfig::language.GetHelp);
    // Draw line
    display.drawHLine(0, 12, 64 - 10);

    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, UserConfig::language.GetHelpLine1);
    display.drawUTF8(0, 38, UserConfig::language.GetHelpLine2);
    display.drawUTF8(0, 62, UserConfig::language.Skip);
    display.sendBuffer();
    displayMutex.unlock();
}
