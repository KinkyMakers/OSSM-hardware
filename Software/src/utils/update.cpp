#include "update.h"

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_heap_caps.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_system.h>

#include "ArduinoJson.h"
#include "constants/LogTags.h"
#include "ossm/Events.h"
#include "ossm/pages/update.h"
#include "ossm/state/state.h"
#include "services/communication/mqtt.h"
#include "structs/Version.h"

// NOTE: logs in updateTask are ESP_LOGW so they are visible in the production
// build (CORE_DEBUG_LEVEL=2 strips ESP_LOGI/D). The free-heap values are the
// whole point of this pass — we want to see how much room TLS actually has.

// Minimal HTTPS GET using the ESP-IDF client with full certificate-bundle
// validation (never setInsecure — that masks TLS-memory issues in prod).
// Returns the HTTP status code, or -1 on transport failure; the response body
// is written to *out when provided.
static int otaHttpGet(const String &url, String *out, int timeoutMs) {
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.timeout_ms = timeoutMs;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        return -1;
    }

    int status = -1;
    if (esp_http_client_open(client, 0) == ESP_OK) {
        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);

        if (out != nullptr) {
            out->clear();
            char buffer[256];
            int read = 0;
            while ((read = esp_http_client_read(client, buffer,
                                                sizeof(buffer) - 1)) > 0) {
                buffer[read] = '\0';
                *out += buffer;
            }
        }
        esp_http_client_close(client);
    }

    esp_http_client_cleanup(client);
    return status;
}

// Fetch the published version for the production channel. Returns currentVersion
// on any failure (incl. a TLS allocation failure under low heap) so a flaky
// network never triggers a spurious update or a crash.
static Version getRemoteVersion() {
    String url = String(OtaConfig::OTA_BASE_URL) + OtaConfig::PRODUCTION_PATH +
                 OtaConfig::VERSION_JSON;

    String payload;
    int status = otaHttpGet(url, &payload, 15000);
    if (status != 200) {
        ESP_LOGW(UPDATE_TAG, "Update check failed (HTTP %d)", status);
        return currentVersion;
    }

    JsonDocument version;
    DeserializationError error = deserializeJson(version, payload);
    if (error) {
        ESP_LOGW(UPDATE_TAG, "Failed to parse version.json: %s", error.c_str());
        return currentVersion;
    }

    return {
        .major = version["major"] | currentVersion.major,
        .minor = version["minor"] | currentVersion.minor,
        .patch = version["patch"] | currentVersion.patch,
    };
}

// Strictly-newer 3-way semver comparison (only updates upward → no downgrades).
static bool isNewer(const Version &remote) {
    if (remote.major != currentVersion.major) {
        return remote.major > currentVersion.major;
    }
    if (remote.minor != currentVersion.minor) {
        return remote.minor > currentVersion.minor;
    }
    return remote.patch > currentVersion.patch;
}

// The whole update runs here, on its own task with a TLS-sized stack (mirrors
// pairingTask). On a successful OTA the device reboots; otherwise it posts
// UpdateUnavailable so the SM shows "no update" and returns to the menu, then
// self-deletes. BLE is intentionally left up — this pass measures whether TLS
// fits alongside it. If TLS can't allocate, it fails gracefully (no crash).
static void updateTask(void *pvParameters) {
    ESP_LOGW(UPDATE_TAG, "Update task started (free heap: %lu, largest block: %lu)",
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    // Free MQTT's TLS memory and stop its competing reconnect retries for the
    // duration of the update. On success the reboot brings MQTT back fresh; on
    // the no-update / failure paths we restart it before returning.
    bool mqttStopped = false;
    if (mqttClient != nullptr) {
        esp_mqtt_client_stop(mqttClient);
        mqttStopped = true;
    }

    Version remote = getRemoteVersion();
    ESP_LOGW(UPDATE_TAG, "Version check done (free heap: %lu)",
             (unsigned long)esp_get_free_heap_size());

    if (!isNewer(remote)) {
        ESP_LOGW(UPDATE_TAG, "No newer version available");
        if (mqttStopped) {
            esp_mqtt_client_start(mqttClient);
        }
        stateMachine->process_event(UpdateUnavailable{});
        vTaskDelete(nullptr);
        return;
    }

    pages::drawUpdating();

    String url = String(OtaConfig::OTA_BASE_URL) + OtaConfig::PRODUCTION_PATH +
                 OtaConfig::FIRMWARE_BIN;
    ESP_LOGW(UPDATE_TAG, "Starting OTA from %s (free heap: %lu, largest block: %lu)",
             url.c_str(), (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = url.c_str();
    httpConfig.timeout_ms = 30000;
    httpConfig.buffer_size = 4096;
    httpConfig.buffer_size_tx = 1024;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;

    esp_err_t ret = esp_https_ota(&httpConfig);
    if (ret == ESP_OK) {
        ESP_LOGW(UPDATE_TAG, "OTA succeeded, restarting...");
        esp_restart();
    }

    ESP_LOGE(UPDATE_TAG, "OTA failed: %s", esp_err_to_name(ret));
    if (mqttStopped) {
        esp_mqtt_client_start(mqttClient);
    }
    stateMachine->process_event(UpdateUnavailable{});
    vTaskDelete(nullptr);
}

// State-machine action (runs in the button-task context). Only spawns the task
// — cheap, no TLS — so the button task's small stack is never the one doing the
// handshake.
void ossmStartUpdate() {
    xTaskCreatePinnedToCore(updateTask, "updateTask",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}
