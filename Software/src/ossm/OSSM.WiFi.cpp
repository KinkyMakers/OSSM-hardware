
#include "OSSM.h"

#include "extensions/u8g2Extensions.h"
#include "utils/uniqueId.h"
#include "constants/URLs.h"
#include "utils/getEfuseMac.h"


void OSSM::drawWiFi() {
    displayMutex.lock();
    display.clearBuffer();

    drawShape::drawQRCode("WIFI:S:OSSM Setup;T:nopass;;");

    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, UserConfig::language.WiFiSetup);
    // Draw line
    display.drawHLine(0, 12, 64 - 10);

    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, UserConfig::language.WiFiSetupLine1);
    display.drawUTF8(0, 38, UserConfig::language.WiFiSetupLine2);
    display.drawUTF8(0, 62, UserConfig::language.Restart);
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
    display.drawUTF8(0, 10, UserConfig::language.Pair);
    display.drawHLine(0, 12, 64 - 10);

    display.setFont(Config::Font::base);
    display.drawUTF8(0, 26, shortId.c_str());

    String url = String(URL_RAD_SHORT) + "?oID=" + shortId;
    drawShape::drawQRCode(url);

    // TODO - Add a spinner here
    display.sendBuffer();
    displayMutex.unlock();


    // prepare the payload.
    DynamicJsonDocument doc(1024);
    String payload;

    // add the device details
    doc["mac"] = getEfuseMacAddress();
    doc["chip"] = ESP.getChipModel();
    doc["md5"] = ESP.getSketchMD5();
    doc["device"] = "OSSM";
    doc["id"] = id;

    serializeJson(doc, payload);

    // print the payload
    ESP_LOGD("PAIRING", "Payload: %s", payload.c_str());

    // POST this payload to the RAD API.
    // this will open a 5 minute pairing window for this device

    HTTPClient http;
    http.begin(String(URL_RAD) + "/api/ossm/pair");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", "Bearer cchzYsEaUEy7zoqfYZO2loHg4pKIcIQAvCo3LW9aKYg=");
    int httpCode = http.POST(payload);

    if (httpCode > 0) {
        ESP_LOGD("PAIRING", "Response: %s", payload.c_str());
    } else {
        ESP_LOGE("PAIRING", "Error: %s", http.errorToString(httpCode).c_str());
    }

    http.end();

    // start a task to wait 5 minutes then throw an error:
    xTaskCreate(
            [](void *pvParameters) {
                OSSM *ossm = (OSSM *) pvParameters;
                int taskStart = millis();
                auto isInCorrectState = [](OSSM *ossm) {
                    // Add any states that you want to support here.
                    return ossm->sm->is("pair"_s) || ossm->sm->is("pair.idle"_s);
                };

                while (isInCorrectState(ossm)) {
                    vTaskDelay(10);
                    int timeElapsed = millis() - taskStart;
                    if (timeElapsed > 300000) {
                        // 5 minutes have passed
                        ossm->errorMessage = "Pairing timed out. Please try again.";
                        ossm->sm->process_event(Error{});
                        break;
                    }

                }

                vTaskDelete(nullptr);

            },
            "buttonTask", 3 * configMINIMAL_STACK_SIZE, this, 1, nullptr);

}
