#ifndef OSSM_UTILS_HTTPS_CLIENT_HPP
#define OSSM_UTILS_HTTPS_CLIENT_HPP

#include <Arduino.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>

#include <utility>
#include <vector>

#if defined(FIRMWARE_USE_IDF_CRT_BUNDLE)
#define OSSM_CRT_BUNDLE_ATTACH esp_crt_bundle_attach
#else
extern "C" esp_err_t arduino_esp_crt_bundle_attach(void *conf);
#define OSSM_CRT_BUNDLE_ATTACH arduino_esp_crt_bundle_attach
#endif

/**
 * One JSON POST over HTTPS with the certificate bundle, using the same
 * esp_http_client configuration as the firmware update runtime. Callers must
 * hold a TlsSession. Returns false and fills `error` on transport failure;
 * a non-200 status is returned in `status` with the body in `response`.
 */
using HttpsHeaders = std::vector<std::pair<const char *, String>>;

inline bool httpsPostJson(const String &url, const String &body, int &status,
                          String &response, String &error,
                          int timeoutMs = 30000,
                          const HttpsHeaders *headers = nullptr) {
    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = timeoutMs;
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = OSSM_CRT_BUNDLE_ATTACH;

    status = 0;
    response = "";
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == nullptr) {
        error = "failed to initialize HTTPS client";
        return false;
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "Accept-Encoding", "identity");
    if (headers != nullptr) {
        for (const auto &header : *headers) {
            esp_http_client_set_header(client, header.first, header.second.c_str());
        }
    }

    bool ok = false;
    const esp_err_t openResult = esp_http_client_open(client, body.length());
    if (openResult != ESP_OK) {
        error = String("HTTPS open failed: ") + esp_err_to_name(openResult);
    } else if (esp_http_client_write(client, body.c_str(), body.length()) !=
               static_cast<int>(body.length())) {
        error = "HTTPS write failed";
    } else {
        esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        char buffer[512];
        int read = 0;
        while ((read = esp_http_client_read(client, buffer, sizeof(buffer))) > 0) {
            if (response.length() + read > 8192) {
                error = "HTTPS response too large";
                response = "";
                break;
            }
            response.concat(buffer, read);
        }
        ok = error.isEmpty();
    }
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    return ok;
}

#endif  // OSSM_UTILS_HTTPS_CLIENT_HPP
