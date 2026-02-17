#include "OSSM.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "constants/Version.h"
#include "extensions/u8g2Extensions.h"
#include "qrcode.h"

void OSSM::checkPairing() {
    macAddress = WiFi.macAddress();
    xTaskCreatePinnedToCore(pairingTask, "pairingTask",
                            6 * configMINIMAL_STACK_SIZE, this, 1, nullptr, 0);
}

void OSSM::drawPairing() {
    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    clearPage(true, true);

    // QR code encoding dashboard URL with pairing code
    String qrUrl = String(RAD_SERVER) + "/app/settings?ossm=" + pairingCode;
    ESP_LOGI("PAIRING", "QR URL: %s (len=%d)", qrUrl.c_str(), qrUrl.length());

    static QRCode qrcode;

    // NOLINTBEGIN(modernize-avoid-c-arrays)
    uint8_t qrcodeData[qrcode_getBufferSize(5)];
    // NOLINTEND(modernize-avoid-c-arrays)

    qrcode_initText(&qrcode, qrcodeData, 5, 0, qrUrl.c_str());

    // QR at scale 1 (37x37px) anchored to bottom-right of 128x64 display
    int qrSize = qrcode.size;  // 37 for version 5
    int qrX = 128 - qrSize - 1;
    int qrY = 64 - qrSize - 1;

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                display.drawPixel(qrX + x, qrY + y);
            }
        }
    }

    // Title (left side, top)
    display.setFont(Config::Font::bold);
    display.drawUTF8(0, 10, "Pair OSSM");
    display.drawHLine(0, 12, qrX - 4);

    // Pairing code in large text
    display.setFont(u8g2_font_helvB14_tf);
    display.drawUTF8(0, 32, pairingCode.c_str());

    // Instructions
    display.setFont(Config::Font::small);
    display.drawUTF8(0, 46, "Enter code in");
    display.drawUTF8(0, 56, "dashboard or");
    display.drawUTF8(0, 64, "scan QR code");

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

void OSSM::pairingTask(void *pvParameters) {
    auto *ossm = static_cast<OSSM *>(pvParameters);

    // Call /api/ossm/auth to register and get pairing code
    HTTPClient http;
    String url = String(RAD_SERVER) + "/api/ossm/auth";
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    JsonDocument doc;
    doc["mac"] = ossm->macAddress;
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
        ossm->pairingCode = resp["pairingCode"].as<String>();

        ESP_LOGI("PAIRING", "Auth response: code=%s", ossm->pairingCode.c_str());
        http.end();

        // Display QR code and pairing code — stays on screen until user
        // presses button (handled by state machine buttonPress = "menu"_s)
        ossm->drawPairing();
    } else {
        ESP_LOGW("PAIRING", "Auth failed with HTTP %d", httpCode);
        http.end();
        ossm->sm->process_event(Error{});
    }

    vTaskDelete(nullptr);
}
