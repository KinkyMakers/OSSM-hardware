#include "pairing.h"

#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include "FirmwareProvenance.h"
#include "pairing_auth.h"

#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/state/state.h"
#include "components/HeaderBar.h"
#include "services/display.h"
#include "ui.h"

namespace {

struct AuthHttpOperations {
    using Config = esp_http_client_config_t;
    using Client = esp_http_client_handle_t;
    static constexpr auto POST = HTTP_METHOD_POST;
    static constexpr auto OK = ESP_OK;
    static constexpr auto attachCertificateBundle = esp_crt_bundle_attach;

    Client init(const Config *config) { return esp_http_client_init(config); }
    int setHeader(Client client, const char *name, const char *value) {
        return esp_http_client_set_header(client, name, value);
    }
    int open(Client client, std::size_t size) {
        return esp_http_client_open(client, static_cast<int>(size));
    }
    int write(Client client, const char *body, std::size_t size) {
        return esp_http_client_write(client, body, static_cast<int>(size));
    }
    std::int64_t fetchHeaders(Client client) { return esp_http_client_fetch_headers(client); }
    int statusCode(Client client) { return esp_http_client_get_status_code(client); }
    int read(Client client, char *buffer, std::size_t size) {
        return esp_http_client_read(client, buffer, static_cast<int>(size));
    }
    bool isComplete(Client client) {
        return esp_http_client_is_complete_data_received(client);
    }
    void close(Client client) { esp_http_client_close(client); }
    void cleanup(Client client) { esp_http_client_cleanup(client); }
};

}  // namespace

namespace pages {

static String pairingCode = "";
static volatile bool paired = false;

static int requestDeviceAuth(bool updatePairingCode) {
    if (WiFi.status() != WL_CONNECTED) {
        return pairing_auth::HTTP_SERVICE_UNAVAILABLE;
    }

    pairing_auth::DeviceIdentity identity;
    identity.mac = WiFi.macAddress().c_str();
    identity.chip = String((uint32_t)ESP.getEfuseMac(), HEX).c_str();
    identity.md5 = ESP.getSketchMD5().c_str();
    identity.version = VERSION;
    identity.provenance = firmware::provenance::currentToken();
    if (!identity.provenance.empty()) {
        identity.provenanceId = firmware::provenance::currentTokenId();
        identity.imageSha256 = firmware::provenance::currentImageSha256();
    }
    AuthHttpOperations http;
    const char *error = nullptr;
    const int httpCode = pairing_auth::requestDeviceAuth(
        http, RAD_SERVER, identity, updatePairingCode, paired, pairingCode, error);
    if (error != nullptr) ESP_LOGW("PAIRING", "Auth request failed: %s", error);
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

    if (httpCode != pairing_auth::HTTP_OK) {
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
            if (httpCode != pairing_auth::HTTP_OK) {
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
