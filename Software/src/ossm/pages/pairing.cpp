#include "pairing.h"

#include <NimBLEDevice.h>
#include <WiFi.h>
#include <esp_heap_caps.h>

#include <ArduinoJson.h>

#include "FirmwareProvenance.h"

#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/pages/update.h"
#include "ossm/state/network.h"
#include "ossm/state/state.h"
#include "components/HeaderBar.h"
#include "services/communication/mqtt.h"
#include "services/display.h"
#include "ui.h"
#include "utils/firmware_md5.h"
#include "utils/https_client.hpp"
#include "utils/tls_session.hpp"

namespace sml = boost::sml;
using namespace sml;

// How often to ask the dashboard whether the code was claimed while the
// pairing page is open. MQTT stays paused for the whole page (TlsSession), so
// each poll has the same heap budget as the first request. 0 disables polling
// and the user exits the page by hand after claiming the code.
#ifndef OSSM_PAIRING_POLL_INTERVAL_MS
#define OSSM_PAIRING_POLL_INTERVAL_MS 10000
#endif

// Background pairing-status poll (started at boot, feeds isOssmPaired()).
#ifndef OSSM_PAIRING_STATUS_INTERVAL_MS
#define OSSM_PAIRING_STATUS_INTERVAL_MS 60000
#endif

namespace pages {

static volatile bool paired = false;

static HttpsHeaders provenanceHeaders() {
    HttpsHeaders headers;
    headers.emplace_back("X-RAD-Firmware-Provenance-Capability", String("1"));
    const auto token = firmware::provenance::currentToken();
    if (!token.empty()) {
        headers.emplace_back("X-RAD-Firmware-Provenance", String(token.c_str()));
        headers.emplace_back("X-RAD-Firmware-Provenance-ID",
                             String(firmware::provenance::currentTokenId().c_str()));
        headers.emplace_back("X-RAD-Firmware-Image-SHA256",
                             String(firmware::provenance::currentImageSha256().c_str()));
    }
    return headers;
}

// POST /api/ossm/auth. Caller holds a TlsSession. Returns the HTTP status
// (0 on transport failure, error filled). Updates `paired`; updates
// networkStatus.pairingCode when asked.
static int requestDeviceAuth(bool updatePairingCode, String &error) {
    JsonDocument doc;
    doc["mac"] = WiFi.macAddress();
    doc["chip"] = String((uint32_t)ESP.getEfuseMac(), HEX);
    doc["md5"] = sketchMd5();
    doc["device"] = "OSSM";
    doc["version"] = VERSION;
    String body;
    serializeJson(doc, body);

    const HttpsHeaders headers = provenanceHeaders();
    const String url = String(RAD_SERVER) + "/api/ossm/auth";
    int status = 0;
    String payload;
    if (!httpsPostJson(url, body, status, payload, error, 30000, &headers)) {
        return 0;
    }
    if (status != 200) return status;

    JsonDocument response;
    if (deserializeJson(response, payload) != DeserializationError::Ok) {
        error = "invalid auth response";
        return 0;
    }
    paired = response["isPaired"].as<bool>();
    networkStatus.isPaired = paired;
    if (updatePairingCode) {
        networkStatus.pairingCode = response["pairingCode"].as<String>();
    }
    return status;
}

static void drawPairingScreen() {
    showHeaderIcons = false;

    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    String qrUrl = String(RAD_SERVER) + "?ossm=" + networkStatus.pairingCode;
    ESP_LOGI("PAIRING", "QR URL: %s (len=%d)", qrUrl.c_str(), qrUrl.length());

    ui::TextPage page = ui::pages::pairingPage;
    page.subtitle = networkStatus.pairingCode.c_str();
    page.qrUrl = qrUrl.c_str();
    ui::drawTextPage(display.getU8g2(), page);

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

static bool onPairingPage() {
    return stateMachine->is("pairing"_s) || stateMachine->is("pairing.idle"_s);
}

static void failPairing(const char *code, const String &detail) {
    ESP_LOGW("PAIRING", "Pairing failed (%s): %s", code, detail.c_str());
    networkStatus.error = code;
    stateMachine->process_event(Error{});
}

// Registers this OSSM with the dashboard and shows the claim code. Runs in
// its own task: the TLS handshake needs more stack than the button task has.
// The whole page holds one TlsSession so MQTT cannot steal the heap between
// the auth call and the is-paired polls.
static void runPairing() {
    networkStatus.clearError();
    networkStatus.clearPairing();

    if (xSemaphoreTake(displayMutex, 200) == pdTRUE) {
        showHeaderIcons = false;
        ui::drawTextPage(display.getU8g2(), ui::pages::pairingConnectingPage);
        refreshPage(true, true);
        xSemaphoreGive(displayMutex);
    }

    if (WiFi.status() != WL_CONNECTED) {
        failPairing("wifi", "Wi-Fi is disconnected");
        return;
    }

    TlsSession tls("pairing");
    if (!tls.ok()) {
        failPairing("low-memory", tls.error());
        return;
    }

    String error;
    const int status = requestDeviceAuth(true, error);
    if (status != 200) {
        failPairing("pairing-failed",
                    error.isEmpty() ? "HTTP " + String(status) : error);
        return;
    }
    ESP_LOGI("PAIRING", "Auth response: code=%s isPaired=%d",
             networkStatus.pairingCode.c_str(), networkStatus.isPaired);

    if (networkStatus.isPaired) {
        stateMachine->process_event(Done{});
        return;
    }

    drawPairingScreen();

#if OSSM_PAIRING_POLL_INTERVAL_MS > 0
    JsonDocument pollDoc;
    pollDoc["macAddress"] = WiFi.macAddress();
    String pollBody;
    serializeJson(pollDoc, pollBody);
    const String pollUrl = String(RAD_SERVER) + "/api/ossm/is-paired";

    while (onPairingPage()) {
        // Sleep in slices so leaving the page ends the task promptly.
        for (int waited = 0; waited < OSSM_PAIRING_POLL_INTERVAL_MS && onPairingPage();
             waited += 250) {
            vTaskDelay(pdMS_TO_TICKS(250));
        }
        if (!onPairingPage()) break;

        String pollPayload;
        String pollError;
        int pollStatus = 0;
        const bool sent =
            httpsPostJson(pollUrl, pollBody, pollStatus, pollPayload, pollError);
        ESP_LOGI("PAIRING", "is-paired poll: sent=%d status=%d %s", sent,
                 pollStatus, pollError.c_str());
        // 200 = paired, 418 = not yet; anything else is retried next round.
        if (sent && pollStatus == 200 && onPairingPage()) {
            paired = true;
            networkStatus.isPaired = true;
            stateMachine->process_event(Done{});
            break;
        }
    }
#endif
}

static void pairingTask(void *pvParameters) {
    runPairing();  // TlsSession restores MQTT when this returns
    vTaskDelete(nullptr);
}

void checkPairing() {
    ESP_LOGI("PAIRING", "checkPairing action triggered");
    pauseMqttForTls();  // frees MQTT's TLS buffers so the task stack fits
    if (xTaskCreatePinnedToCore(pairingTask, "pairingTask",
                                20 * configMINIMAL_STACK_SIZE, nullptr, 1,
                                nullptr, 0) != pdPASS) {
        resumeMqttAfterTls();
        failPairing("low-memory", "could not create pairing task");
    }
}

// Boot-time background poll: asks the dashboard once a minute whether this
// OSSM is paired, until it is. It never pauses MQTT; when the heap cannot
// take a TLS handshake alongside MQTT and BLE it skips the round instead of
// failing (the pairing page, which does pause MQTT, also sets `paired`).
static void pairingStatusTask(void *pvParameters) {
    while (!paired) {
        // Once-a-minute memory trend: the number that decides whether any
        // TLS job can run, next to the BLE central count that drives it.
        ESP_LOGW("MEM", "idle: internal free=%u largest=%u bleCentrals=%u mqtt=%d",
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL |
                                                            MALLOC_CAP_8BIT),
                 (unsigned)(NimBLEDevice::getServer() != nullptr
                                ? NimBLEDevice::getServer()->getConnectedCount()
                                : 0),
                 mqttConnected ? 1 : 0);
        if (WiFi.status() == WL_CONNECTED) {
            TlsSession budget("pairing-status", /*pauseMqtt=*/false);
            if (budget.ok()) {
                String error;
                const int status = requestDeviceAuth(false, error);
                if (status != 200) {
                    ESP_LOGW("PAIRING", "Pairing status check failed: HTTP %d %s",
                             status, error.c_str());
                } else {
                    ESP_LOGI("PAIRING", "Pairing status: isPaired=%d", paired);
                }
            } else {
                ESP_LOGD("PAIRING", "Pairing status check skipped: %s",
                         budget.error().c_str());
            }
        }

        if (!paired) {
            vTaskDelay(pdMS_TO_TICKS(OSSM_PAIRING_STATUS_INTERVAL_MS));
        }
    }

    vTaskDelete(nullptr);
}

bool isOssmPaired() { return paired; }

void startPairingStatusCheck() {
    if (xTaskCreatePinnedToCore(pairingStatusTask, "pairingStatusTask",
                                20 * configMINIMAL_STACK_SIZE, nullptr, 1,
                                nullptr, 0) != pdPASS) {
        ESP_LOGE("PAIRING", "Could not create pairing status task");
    }
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

void drawPairingFailed() {
    showHeaderIcons = true;

    if (xSemaphoreTake(displayMutex, 200) != pdTRUE) {
        return;
    }

    ui::TextPage page = ui::pages::pairingFailedPage;
    page.subtitle = networkErrorReason(networkStatus.error);
    ui::drawTextPage(display.getU8g2(), page);

    refreshPage(true, true);
    xSemaphoreGive(displayMutex);
}

}  // namespace pages
