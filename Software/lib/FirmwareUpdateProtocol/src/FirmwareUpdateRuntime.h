#pragma once

#include "FirmwareUpdateCore.h"
#include "FirmwareProvenance.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Arduino.h>
#include <Update.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <mbedtls/sha256.h>

#if defined(FIRMWARE_USE_IDF_CRT_BUNDLE)
#define FIRMWARE_CRT_BUNDLE_ATTACH esp_crt_bundle_attach
#else
#define FIRMWARE_CRT_BUNDLE_ATTACH arduino_esp_crt_bundle_attach
#endif

namespace firmware {

namespace detail {

struct HttpClient {
    esp_http_client_handle_t client;

    explicit HttpClient(const esp_http_client_config_t &config)
        : client(esp_http_client_init(&config)) {}

    bool initialized() const { return client != nullptr; }
    void setHeader(const char *name, const char *value) {
        if (initialized()) esp_http_client_set_header(client, name, value);
    }
    bool open(std::size_t length) {
        return esp_http_client_open(client, length) == ESP_OK;
    }
    int write(const char *data, std::size_t length) {
        return esp_http_client_write(client, data, length);
    }
    std::int64_t fetchHeaders() { return esp_http_client_fetch_headers(client); }
    int status() const { return esp_http_client_get_status_code(client); }
    int read(char *data, std::size_t length) {
        return esp_http_client_read(client, data, length);
    }
    bool isComplete() const {
        return esp_http_client_is_complete_data_received(client);
    }
    void close() { esp_http_client_close(client); }
    void cleanup() { esp_http_client_cleanup(client); }
};

struct Sha256 {
    mbedtls_sha256_context context;

    void init() { mbedtls_sha256_init(&context); }
    bool start() { return mbedtls_sha256_starts_ret(&context, 0) == 0; }
    bool update(const unsigned char *data, std::size_t length) {
        return mbedtls_sha256_update_ret(&context, data, length) == 0;
    }
    bool finish(unsigned char digest[32]) {
        return mbedtls_sha256_finish_ret(&context, digest) == 0;
    }
    void free() { mbedtls_sha256_free(&context); }
};

}  // namespace detail

inline bool postCheck(const char *apiBaseUrl, const DeviceReport &report,
                      Decision &decision, String &error) {
    String url = apiBaseUrl;
    if (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/api/firmware/v1/check";

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 30000;
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = FIRMWARE_CRT_BUNDLE_ATTACH;

    detail::HttpClient http(config);
    if (http.initialized()) {
        http.setHeader("Content-Type", "application/json");
        http.setHeader("Accept-Encoding", "identity");
        http.setHeader("X-RAD-Firmware-Provenance-Capability", "1");
        if (!report.firmwareProvenance.empty()) {
            const std::string provenanceId =
                provenance::tokenId(report.firmwareProvenance);
            http.setHeader("X-RAD-Firmware-Provenance",
                           report.firmwareProvenance.c_str());
            http.setHeader("X-RAD-Firmware-Provenance-ID", provenanceId.c_str());
        }
        if (!report.firmwareHash.empty())
            http.setHeader("X-RAD-Firmware-Image-SHA256", report.firmwareHash.c_str());
    }

    return postCheckWith(http, report, decision, error);
}

inline bool installStreamedArtifact(const Artifact &artifact, int updateCommand,
                                    String &error) {
    esp_http_client_config_t config = {};
    config.url = artifact.url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 60000;
    config.buffer_size = 4096;
    config.crt_bundle_attach = FIRMWARE_CRT_BUNDLE_ATTACH;

    detail::HttpClient http(config);
    http.setHeader("Accept-Encoding", "identity");
    detail::Sha256 sha;
    return installStreamedArtifactWith(artifact, updateCommand, http, Update,
                                       sha, error);
}

inline bool installApplicationAndFilesystem(const Decision &decision,
                                            String &error) {
    struct Installer {
        bool install(const Artifact &artifact, int command, String &error) {
            return installStreamedArtifact(artifact, command, error);
        }
    } installer;
    return installApplicationAndFilesystemWith(decision, U_FLASH, U_SPIFFS,
                                               installer, error);
}

}  // namespace firmware

#endif
