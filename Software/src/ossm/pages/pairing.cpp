#include "pairing.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "constants/Config.h"
#include "constants/Version.h"
#include "extensions/u8g2Extensions.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "qrcode.h"
#include "services/display.h"

namespace sml = boost::sml;
using namespace sml;

namespace pages {

static String pairingCode = "";

static void drawPairingScreen() {
    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    clearPage(true, true);

    String qrUrl = String(RAD_SERVER) + "/app/settings?ossm=" + pairingCode;
    ESP_LOGI("PAIRING", "QR URL: %s (len=%d)", qrUrl.c_str(), qrUrl.length());

    static QRCode qrcode;

    // NOLINTBEGIN(modernize-avoid-c-arrays)
    uint8_t qrcodeData[qrcode_getBufferSize(5)];
    // NOLINTEND(modernize-avoid-c-arrays)

    qrcode_initText(&qrcode, qrcodeData, 5, 0, qrUrl.c_str());

    int qrSize = qrcode.size;
    int qrX = 128 - qrSize - 1;
    int qrY = 64 - qrSize - 1;

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                display.drawPixel(qrX + x, qrY + y);
            }
        }
    }

    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, "Pair OSSM");
    display.drawHLine(0, 12, qrX - 4);

    display.setFont(u8g2_font_helvB14_tf);
    display.drawUTF8(0, 32, pairingCode.c_str());

    display.setFont(Config::Font::small);
    display.drawUTF8(0, 46, "Enter code in");
    display.drawUTF8(0, 56, "dashboard or");
    display.drawUTF8(0, 64, "scan QR code");

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
