#include "OSSM.h"

#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>

#include <ArduinoJson.h>

#include "constants/Version.h"
#include "extensions/u8g2Extensions.h"
#include "qrcode.h"

static const char *NVS_NAMESPACE = "ossm_pair";
static const char *NVS_KEY_PAIRED = "isPaired";
static const char *NVS_KEY_MAC = "mac";

static constexpr unsigned long PAIRING_POLL_INTERVAL_MS = 2000;
static constexpr unsigned long PAIRING_TIMEOUT_MS = 300000;  // 5 minutes

void OSSM::loadPairingState() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, true);
    isPairedToOwner = prefs.getBool(NVS_KEY_PAIRED, false);
    prefs.end();
    ESP_LOGI("PAIRING", "Loaded pairing state: %s",
             isPairedToOwner ? "paired" : "not paired");
}

void OSSM::savePairingState() {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putBool(NVS_KEY_PAIRED, true);
    prefs.putString(NVS_KEY_MAC, macAddress);
    prefs.end();
    ESP_LOGI("PAIRING", "Saved pairing state for MAC %s", macAddress.c_str());
}

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

    static QRCode qrcode;
    const int scale = 2;

    // NOLINTBEGIN(modernize-avoid-c-arrays)
    uint8_t qrcodeData[qrcode_getBufferSize(5)];
    // NOLINTEND(modernize-avoid-c-arrays)

    qrcode_initText(&qrcode, qrcodeData, 5, 0, qrUrl.c_str());

    int yOffset = 64 - qrcode.size * scale;
    int xOffset = 128 - qrcode.size * scale;

    for (uint8_t y = 0; y < qrcode.size; y++) {
        for (uint8_t x = 0; x < qrcode.size; x++) {
            if (qrcode_getModule(&qrcode, x, y)) {
                display.drawBox(xOffset + x * scale, yOffset + y * scale,
                                scale, scale);
            }
        }
    }

    // Title
    display.setFont(Config::Font::bold);
    drawStr::centered(10, "Pair OSSM");
    display.drawHLine(0, 12, xOffset - 10);

    // Pairing code in large text
    display.setFont(u8g2_font_helvB14_tf);
    display.drawUTF8(0, 34, pairingCode.c_str());

    // Instructions
    display.setFont(Config::Font::small);
    display.drawUTF8(0, 48, "Enter code in");
    display.drawUTF8(0, 58, "dashboard. Press=skip");

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

void OSSM::pairingTask(void *pvParameters) {
    auto *ossm = static_cast<OSSM *>(pvParameters);

    // Step 1: Call /api/ossm/auth to register and get pairing code
    {
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

        int httpCode = http.POST(body);

        if (httpCode == 200) {
            String payload = http.getString();
            JsonDocument resp;
            deserializeJson(resp, payload);
            ossm->pairingCode = resp["pairingCode"].as<String>();
            bool alreadyPaired = resp["isPaired"] | false;

            ESP_LOGI("PAIRING", "Auth response: code=%s, isPaired=%d",
                     ossm->pairingCode.c_str(), alreadyPaired);

            if (alreadyPaired) {
                ossm->isPairedToOwner = true;
                ossm->savePairingState();
                ossm->sm->process_event(Done{});
                http.end();
                vTaskDelete(nullptr);
                return;
            }
        } else {
            ESP_LOGW("PAIRING", "Auth failed with HTTP %d", httpCode);
            ossm->sm->process_event(Error{});
            http.end();
            vTaskDelete(nullptr);
            return;
        }

        http.end();
    }

    // Step 2: Draw the pairing screen
    ossm->drawPairing();

    // Step 3: Poll /api/ossm/is-paired every 2 seconds
    auto isInPairingState = [](OSSM *o) {
        return o->sm->is("pairing"_s) || o->sm->is("pairing.idle"_s);
    };

    unsigned long startTime = millis();

    while (isInPairingState(ossm)) {
        if (millis() - startTime > PAIRING_TIMEOUT_MS) {
            ESP_LOGI("PAIRING", "Pairing timeout after %lu ms",
                     PAIRING_TIMEOUT_MS);
            ossm->sm->process_event(Error{});
            break;
        }

        HTTPClient http;
        String url = String(RAD_SERVER) + "/api/ossm/is-paired";
        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        JsonDocument doc;
        doc["macAddress"] = ossm->macAddress;

        String body;
        serializeJson(doc, body);

        int httpCode = http.POST(body);
        http.end();

        if (httpCode == 200) {
            ESP_LOGI("PAIRING", "Device is paired!");
            ossm->isPairedToOwner = true;
            ossm->savePairingState();
            ossm->sm->process_event(Done{});
            break;
        }

        // 418 = not yet paired, any other error = keep trying
        if (httpCode != 418) {
            ESP_LOGW("PAIRING", "Unexpected is-paired response: %d", httpCode);
        }

        vTaskDelay(PAIRING_POLL_INTERVAL_MS / portTICK_PERIOD_MS);
    }

    vTaskDelete(nullptr);
}
