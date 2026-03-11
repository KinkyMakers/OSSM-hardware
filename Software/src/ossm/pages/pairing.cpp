#include "pairing.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "components/HeaderBar.h"
#include "services/display.h"
#include "ui.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

static String pairingCode = "";

static void drawPairingScreen() {
    showHeaderIcons = false;

    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    String qrUrl = String(RAD_SERVER) + "?ossm=" + pairingCode;
    ESP_LOGI("PAIRING", "QR URL: %s (len=%d)", qrUrl.c_str(), qrUrl.length());

    ui::TextPage page = ui::pages::pairingPage;
    page.subtitle = pairingCode.c_str();
    page.qrUrl = qrUrl.c_str();
    ui::drawTextPage(display.getU8g2(), page);

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

static void pairingTask(void *pvParameters) {
    if (xSemaphoreTake(displayMutex, 200) == pdTRUE) {
        showHeaderIcons = false;
        ui::drawTextPage(display.getU8g2(), ui::pages::pairingConnectingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

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

    if (httpCode != 200) {
        ESP_LOGW("PAIRING", "Auth failed with HTTP %d", httpCode);
        http.end();
        stateMachine->process_event(Error{});
        vTaskDelete(nullptr);
        return;
    }

    String payload = http.getString();
    http.end();

    JsonDocument resp;
    deserializeJson(resp, payload);
    pairingCode = resp["pairingCode"].as<String>();
    bool isPaired = resp["isPaired"].as<bool>();
    ESP_LOGI("PAIRING", "Auth response: code=%s isPaired=%d", pairingCode.c_str(), isPaired);

    if (isPaired) {
        stateMachine->process_event(Done{});
        vTaskDelete(nullptr);
        return;
    }

    drawPairingScreen();

    // TODO: replace with MQTT subscription to ossm/{macAddress}/paired
    // Polling disabled — causes SSL memory exhaustion when BLE + MQTT TLS are active.
    // User must press the button to exit after pairing on the website.

    // auto isInCorrectState = []() {
    //     return stateMachine->is("pairing"_s) ||
    //            stateMachine->is("pairing.idle"_s);
    // };

    // while (isInCorrectState()) {
    //     vTaskDelay(pdMS_TO_TICKS(1000));

    //     if (!isInCorrectState()) break;

    //     HTTPClient pollHttp;
    //     pollHttp.begin(String(RAD_SERVER) + "/api/ossm/is-paired");
    //     pollHttp.addHeader("Content-Type", "application/json");

    //     JsonDocument pollDoc;
    //     pollDoc["macAddress"] = macAddress;
    //     String pollBody;
    //     serializeJson(pollDoc, pollBody);

    //     int pollCode = pollHttp.POST(pollBody);
    //     pollHttp.end();

    //     ESP_LOGI("PAIRING", "is-paired poll: %d", pollCode);

    //     if (pollCode == 200) {
    //         stateMachine->process_event(Done{});
    //         break;
    //     }
    // }

    vTaskDelete(nullptr);
}

void checkPairing() {
    ESP_LOGI("PAIRING", "checkPairing action triggered");
    xTaskCreatePinnedToCore(pairingTask, "pairingTask",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}

void drawPairingSuccess() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    ui::drawTextPage(display.getU8g2(), ui::pages::pairingSuccessPage);

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

}  // namespace pages
