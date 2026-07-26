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
static volatile bool paired = false;

static int requestDeviceAuth(bool updatePairingCode) {
    if (WiFi.status() != WL_CONNECTED) {
        return HTTP_CODE_SERVICE_UNAVAILABLE;
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

    int httpCode = http.POST(body);
    if (httpCode == HTTP_CODE_OK) {
        JsonDocument response;
        DeserializationError jsonError =
            deserializeJson(response, http.getString());
        if (jsonError) {
            ESP_LOGW("PAIRING", "Invalid auth response: %s",
                     jsonError.c_str());
            http.end();
            return HTTP_CODE_INTERNAL_SERVER_ERROR;
        }

        paired = response["isPaired"].as<bool>();
        if (updatePairingCode) {
            pairingCode = response["pairingCode"].as<String>();
        }
    }
    http.end();
    return httpCode;
}

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

    int httpCode = requestDeviceAuth(true);

    if (httpCode != HTTP_CODE_OK) {
        ESP_LOGW("PAIRING", "Auth failed with HTTP %d", httpCode);
        stateMachine->process_event(Error{});
        vTaskDelete(nullptr);
        return;
    }

    ESP_LOGI("PAIRING", "Auth response: code=%s isPaired=%d",
             pairingCode.c_str(), paired);

    if (paired) {
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

static void pairingStatusTask(void *pvParameters) {
    while (!paired) {
        if (WiFi.status() == WL_CONNECTED) {
            int httpCode = requestDeviceAuth(false);
            if (httpCode != HTTP_CODE_OK) {
                ESP_LOGW("PAIRING", "Pairing status check failed with HTTP %d",
                         httpCode);
            } else {
                ESP_LOGI("PAIRING", "Pairing status: isPaired=%d", paired);
            }
        }

        if (!paired) {
            vTaskDelay(pdMS_TO_TICKS(60000));
        }
    }

    vTaskDelete(nullptr);
}

bool isOssmPaired() { return paired; }

void startPairingStatusCheck() {
    xTaskCreatePinnedToCore(pairingStatusTask, "pairingStatusTask",
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
