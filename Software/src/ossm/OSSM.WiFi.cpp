
#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "qrcode.h"
#include "utils/uniqueId.h"
#include "constants/URLs.h"

void OSSM::drawWiFi() {
    displayMutex.lock();
    display.clearBuffer();

    drawShape::drawQRCode("WIFI:S:OSSM Setup;T:nopass;;");

    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, UserConfig::language.WiFiSetup.c_str());
    // Draw line
    display.drawHLine(0, 12, 64 - 10);

    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, UserConfig::language.WiFiSetupLine1.c_str());
    display.drawUTF8(0, 38, UserConfig::language.WiFiSetupLine2.c_str());
    display.drawUTF8(0, 62, UserConfig::language.Restart.c_str());
    display.sendBuffer();
    displayMutex.unlock();

    wm.startConfigPortal("OSSM Setup");
}

void OSSM::drawPairing() {

    String id = getId();
    String shortId = id.substring(id.length() - 5);
    // get the last 5 digits of the id


    displayMutex.lock();
    display.clearBuffer();

    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, UserConfig::language.Pair.c_str());
    display.drawHLine(0, 12, 64 - 10);

    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, shortId.c_str());


    String url = String(URL_RAD_SHORT) + "?id=" + shortId;
    drawShape::drawQRCode(url);

    // TODO - Add a spinner here
    display.sendBuffer();
    displayMutex.unlock();


    // prepare the payload.
    DynamicJsonDocument doc(1024);
    String payload;

    // add the device details
    doc["mac"] = ESP.getEfuseMac();
    doc["chip"] = ESP.getChipModel();
    doc["md5"] = ESP.getSketchMD5();
    doc["device"] = "OSSM";
    doc["id"] = id;

    serializeJson(doc, payload);

    // print the payload
    ESP_LOGD("PAIRING", "Payload: %s", payload.c_str());


}
