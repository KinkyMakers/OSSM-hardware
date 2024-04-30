#include "OSSM.h"

#include "constants/UserConfig.h"
#include "extensions/u8g2Extensions.h"

void OSSM::startStrokeEngine() {
    displayMutex.lock();
    display.clearBuffer();

    drawStr::title(UserConfig::language.StrokeEngine);
    drawStr::multiLine(0, 40, UserConfig::language.InDevelopment);

    display.sendBuffer();
    displayMutex.unlock();
}