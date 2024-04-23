#include "OSSM.h"

#include "Arduino.h"
#include "extensions/u8g2Extensions.h"
void OSSM::drawUpdate() {
    display.clearBuffer();
    String title = "Update";
    drawStr::title(title);
    display.sendBuffer();
}
