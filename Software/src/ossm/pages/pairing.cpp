#include "pairing.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "services/display.h"
#include "ui.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

static String pairingCode = "";

static void drawPairingScreen() {
    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    String qrUrl = String(RAD_SERVER) + "/OSSM/" + pairingCode;
    qrUrl.toUpperCase();
    ESP_LOGI("PAIRING", "QR URL: %s (len=%d)", qrUrl.c_str(), qrUrl.length());

    ui::TextPage page = ui::pages::pairingPage;
    page.subtitle = pairingCode.c_str();
    page.qrUrl = qrUrl.c_str();
    ui::drawTextPage(display.getU8g2(), page);

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

static void pairingTask(void *pvParameters) {
    String macAddress = WiFi.macAddress();

    HTTPClient http;
    String url = String(RAD_SERVER) + "/api/ossm/auth";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["mac"] = macAddress;
    doc["chip"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    doc["md5"] = ESP.getSketchMD5();
    doc["device"] = "OSSM";
    doc["version"] = VERSION;

    String body;
    serializeJson(doc, body);

    ESP_LOGI("PAIRING", "POST %s", url.c_str());
    int httpCode = http.POST(body);

    if (httpCode == 200) {
        String payload = http.getString();
        JsonDocument resp;
        deserializeJson(resp, payload);
        pairingCode = resp["pairingCode"].as<String>();

        ESP_LOGI("PAIRING", "Auth response: code=%s", pairingCode.c_str());
        http.end();

        drawPairingScreen();
    } else {
        ESP_LOGW("PAIRING", "Auth failed with HTTP %d", httpCode);
        http.end();
        stateMachine->process_event(Error{});
    }

    vTaskDelete(nullptr);
}

void checkPairing() {
    ESP_LOGI("PAIRING", "checkPairing action triggered");
    xTaskCreatePinnedToCore(pairingTask, "pairingTask",
                            6 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}

}  // namespace pages
