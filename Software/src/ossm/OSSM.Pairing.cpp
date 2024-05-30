#include "OSSM.h"

#include "Arduino.h"
#include "extensions/u8g2Extensions.h"
void OSSM::drawPairing() {
    displayMutex.lock();
    display.clearBuffer();
    String title = "PAIRING :D";
    drawStr::title(title);

    // TODO - Add a spinner here
    display.sendBuffer();
    displayMutex.unlock();
}