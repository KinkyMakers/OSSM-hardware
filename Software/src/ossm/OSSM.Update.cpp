#include "OSSM.h"

#include "Arduino.h"
#include "extensions/u8g2Extensions.h"
void OSSM::drawUpdate() {
    displayMutex.lock();
    display.clearBuffer();
    drawStr::title(F("Checking for update..."));

    // TODO - Add a spinner here
    display.sendBuffer();
    displayMutex.unlock();
}

void OSSM::drawNoUpdate() {
    displayMutex.lock();
    display.clearBuffer();
    drawStr::title(F("No Update Available"));
    display.drawUTF8(0, 62, UserConfig::language.Skip);
    display.sendBuffer();
    displayMutex.unlock();
}

void OSSM::drawUpdating() {
    displayMutex.lock();
    display.clearBuffer();
    drawStr::title(F("Updating OSSM..."));
    drawStr::multiLine(0, 24, UserConfig::language.UpdateMessage);
    display.sendBuffer();
    displayMutex.unlock();
}
