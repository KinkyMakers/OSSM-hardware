#include "OSSM.h"

#include "Arduino.h"
#include "extensions/u8g2Extensions.h"
void OSSM::drawUpdate() {
    displayMutex.lock();
    display.clearBuffer();
    String title = "Checking for update...";
    drawStr::title(title);

    // TODO - Add a spinner here
    display.sendBuffer();
    displayMutex.unlock();
}

void OSSM::drawNoUpdate() {
    displayMutex.lock();
    display.clearBuffer();
    String title = "No Update Available";
    drawStr::title(title);
    display.drawUTF8(0, 62, UserConfig::language.Skip.c_str());
    display.sendBuffer();
    displayMutex.unlock();
}

void OSSM::drawUpdating() {
    displayMutex.lock();
    display.clearBuffer();
    String title = "Updating OSSM...";
    drawStr::title(title);
    drawStr::multiLine(0, 24, UserConfig::language.UpdateMessage);
    display.sendBuffer();
    displayMutex.unlock();
}
