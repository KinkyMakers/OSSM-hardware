#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_log.h>
#include <esp_system.h>

#include "ArduinoJson.h"
#include "constants/LogTags.h"
#include "structs/Version.h"

// Firmware update host. Single source of truth shared with the webflasher: the
// Supabase "production" channel. The device fetches version.json, decides for
// itself whether a newer build exists, and downloads firmware.bin from the same
// place over validated HTTPS. To move hosts later (e.g. GitHub Pages), only
// OTA_BASE_URL changes.
namespace OtaConfig {
static const char *OTA_BASE_URL =
    "https://acjajruwevyyatztbkdf.supabase.co/storage/v1/object/public/"
    "ossm-firmware/";
static const char *PRODUCTION_PATH = "production/";
static const char *VERSION_JSON = "version.json";
static const char *FIRMWARE_BIN = "firmware.bin";
}  // namespace OtaConfig

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

// Fetch the published version for the production channel and compare it against
// the firmware baked into this build. On any failure we return currentVersion
// so a flaky network never triggers a spurious update or a crash.
static Version getRemoteVersion() {
    if (WiFiClass::status() != WL_CONNECTED) {
        ESP_LOGD(UPDATE_TAG, "Not connected to WiFi");
        return currentVersion;
    }

    String url = String(OtaConfig::OTA_BASE_URL) + OtaConfig::PRODUCTION_PATH +
                 OtaConfig::VERSION_JSON;
    ESP_LOGD(UPDATE_TAG, "Checking for updates at %s", url.c_str());

    String payload;
    int status = otaHttpGet(url, &payload, 15000);
    if (status != 200) {
        ESP_LOGD(UPDATE_TAG, "Update check failed (HTTP %d)", status);
        return currentVersion;
    }

    ESP_LOGD(UPDATE_TAG, "version.json: %s", payload.c_str());

    // version.json: { "version": "1.0.28", "major": 1, "minor": 0, "patch": 28 }
    JsonDocument version;
    DeserializationError error = deserializeJson(version, payload);
    if (error) {
        ESP_LOGD(UPDATE_TAG, "Failed to parse version.json: %s", error.c_str());
        return currentVersion;
    }

    return {
        .major = version["major"] | currentVersion.major,
        .minor = version["minor"] | currentVersion.minor,
        .patch = version["patch"] | currentVersion.patch,
    };
}

// State-machine guard ("update.checking" -> "update.updating"). Only true when
// the remote build is strictly newer, which prevents downgrades.
static auto isUpdateAvailable = []() {
    Version remoteVersion = getRemoteVersion();
    if (remoteVersion.major != currentVersion.major) {
        return remoteVersion.major > currentVersion.major;
    }
    if (remoteVersion.minor != currentVersion.minor) {
        return remoteVersion.minor > currentVersion.minor;
    }
    return remoteVersion.patch > currentVersion.patch;
};

// State-machine action: download + flash the production firmware over HTTPS,
// then reboot into it. esp_https_ota does not auto-reboot, so we restart here
// on success (the Arduino httpUpdate path used to reboot implicitly).
auto updateOSSM = []() {
    String url = String(OtaConfig::OTA_BASE_URL) + OtaConfig::PRODUCTION_PATH +
                 OtaConfig::FIRMWARE_BIN;
    ESP_LOGI(UPDATE_TAG, "Starting OTA from %s (free heap: %lu)", url.c_str(),
             (unsigned long)esp_get_free_heap_size());

    esp_http_client_config_t httpConfig = {};
    httpConfig.url = url.c_str();
    httpConfig.timeout_ms = 30000;
    httpConfig.buffer_size = 4096;
    httpConfig.buffer_size_tx = 1024;
    httpConfig.crt_bundle_attach = esp_crt_bundle_attach;

    esp_err_t ret = esp_https_ota(&httpConfig);
    if (ret == ESP_OK) {
        ESP_LOGI(UPDATE_TAG, "OTA succeeded, restarting...");
        esp_restart();
    }

    ESP_LOGE(UPDATE_TAG, "OTA failed: %s", esp_err_to_name(ret));
};

#endif  // SOFTWARE_UPDATE_H
