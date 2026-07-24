#pragma once

#include "FirmwareUpdateProtocol.h"

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

inline bool postCheck(const char *apiBaseUrl, const DeviceReport &report,
                      Decision &decision, String &error) {
    String url = apiBaseUrl;
    if (url.endsWith("/")) url.remove(url.length() - 1);
    url += "/api/firmware/v1/check";
    const std::string body = serializeReport(report);

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 30000;
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = FIRMWARE_CRT_BUNDLE_ATTACH;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        error = "failed to initialize HTTPS client";
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    bool success = false;
    if (esp_http_client_open(client, body.size()) != ESP_OK ||
        esp_http_client_write(client, body.data(), body.size()) !=
            static_cast<int>(body.size())) {
        error = "firmware check request failed";
    } else {
        esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        String response;
        char buffer[512];
        int read = 0;
        while ((read = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            if (response.length() + read > 32768) {
                error = "firmware response is too large";
                response.clear();
                break;
            }
            response.concat(buffer, read);
        }
        if (status != 200) {
            error = "firmware check returned HTTP " + String(status);
        } else if (!response.isEmpty()) {
            std::string parseError;
            success = parseDecision(response.c_str(), report.deviceType,
                                    decision, parseError);
            if (!success) error = parseError.c_str();
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    if (success) {
        std::string validationError;
        if (!validateDecisionForReport(report, decision, validationError)) {
            error = validationError.c_str();
            return false;
        }
    }
    return success;
}

inline std::string sha256Hex(const unsigned char digest[32]) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    std::string output(64, '0');
    for (std::size_t index = 0; index < 32; ++index) {
        output[index * 2] = HEX_DIGITS[(digest[index] >> 4) & 0x0f];
        output[index * 2 + 1] = HEX_DIGITS[digest[index] & 0x0f];
    }
    return output;
}

inline bool installStreamedArtifact(const Artifact &artifact, int updateCommand,
                                    String &error) {
    esp_http_client_config_t config = {};
    config.url = artifact.url.c_str();
    config.method = HTTP_METHOD_GET;
    config.timeout_ms = 60000;
    config.buffer_size = 4096;
    config.crt_bundle_attach = FIRMWARE_CRT_BUNDLE_ATTACH;

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        error = "failed to initialize artifact HTTPS client";
        return false;
    }
    esp_http_client_set_header(client, "Accept-Encoding", "identity");

    bool success = false;
    if (esp_http_client_open(client, 0) != ESP_OK) {
        error = "artifact connection failed";
    } else {
        const auto contentLength = esp_http_client_fetch_headers(client);
        const int status = esp_http_client_get_status_code(client);
        if (status != 200) {
            error = "artifact returned HTTP " + String(status);
        } else if (contentLength >= 0 &&
                   static_cast<std::uint64_t>(contentLength) !=
                       artifact.sizeBytes) {
            error = "artifact content length mismatch";
        } else if (!Update.begin(artifact.sizeBytes, updateCommand)) {
            error = "update partition rejected artifact";
        } else {
            mbedtls_sha256_context sha;
            mbedtls_sha256_init(&sha);
            mbedtls_sha256_starts_ret(&sha, 0);
            std::uint32_t total = 0;
            unsigned char buffer[4096];
            int read = 0;
            bool streamOk = true;
            while ((read = esp_http_client_read(
                        client, reinterpret_cast<char *>(buffer),
                        sizeof(buffer))) > 0) {
                if (total + read > artifact.sizeBytes ||
                    Update.write(buffer, read) != static_cast<std::size_t>(read)) {
                    streamOk = false;
                    error = "artifact write failed";
                    break;
                }
                mbedtls_sha256_update_ret(&sha, buffer, read);
                total += read;
            }
            unsigned char digest[32] = {};
            mbedtls_sha256_finish_ret(&sha, digest);
            mbedtls_sha256_free(&sha);

            if (!streamOk || read < 0 || total != artifact.sizeBytes) {
                if (error.isEmpty()) error = "artifact download was incomplete";
                Update.abort();
            } else if (sha256Hex(digest) != artifact.sha256) {
                error = "artifact SHA-256 mismatch";
                Update.abort();
            } else if (!Update.end(true)) {
                error = "artifact finalization failed";
            } else {
                success = true;
            }
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return success;
}

inline bool installApplicationAndFilesystem(const Decision &decision,
                                            String &error) {
    bool applicationInstalled = false;
    for (std::size_t index = 0; index < decision.artifactCount; ++index) {
        const Artifact &artifact = decision.artifacts[index];
        if (artifact.role == "filesystem") {
            if (!installStreamedArtifact(artifact, U_SPIFFS, error)) return false;
        } else if (artifact.role == "application") {
            if (!installStreamedArtifact(artifact, U_FLASH, error)) return false;
            applicationInstalled = true;
        } else {
            error = ("unsupported installable artifact role: " + artifact.role).c_str();
            return false;
        }
    }
    if (!applicationInstalled) {
        error = "release has no application artifact";
        return false;
    }
    return true;
}

}  // namespace firmware

#endif
