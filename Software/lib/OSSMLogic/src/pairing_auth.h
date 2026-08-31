#pragma once

#include <ArduinoJson.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace pairing_auth {

constexpr int HTTP_OK = 200;
constexpr int HTTP_INTERNAL_SERVER_ERROR = 500;
constexpr int HTTP_SERVICE_UNAVAILABLE = 503;
constexpr int REQUEST_TIMEOUT_MS = 5000;
constexpr std::size_t MAX_RESPONSE_BYTES = 32768;

struct DeviceIdentity {
    std::string mac;
    std::string chip;
    std::string md5;
    std::string version;
    std::string provenance;
    std::string provenanceId;
    std::string imageSha256;
};

// Operations are thin ESP-IDF bindings in firmware and injected transports in
// native tests. Request construction, parsing and state changes stay identical.
template <typename Http, typename Paired, typename CodeString>
int requestDeviceAuth(Http &http, const char *apiBaseUrl,
                      const DeviceIdentity &identity, bool updatePairingCode,
                      Paired &paired, CodeString &pairingCode,
                      const char *&error) {
    error = nullptr;
    const std::string url = std::string(apiBaseUrl) + "/api/ossm/auth";
    JsonDocument document;
    document["mac"] = identity.mac;
    document["chip"] = identity.chip;
    document["md5"] = identity.md5;
    document["device"] = "OSSM";
    document["version"] = identity.version;
    std::string body;
    serializeJson(document, body);

    typename Http::Config config{};
    config.url = url.c_str();
    config.method = Http::POST;
    config.timeout_ms = REQUEST_TIMEOUT_MS;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = Http::attachCertificateBundle;
    config.skip_cert_common_name_check = false;
    config.disable_auto_redirect = true;
    const auto client = http.init(&config);
    if (client == nullptr) {
        error = "failed to initialize auth HTTPS client";
        return -1;
    }
    struct Cleanup {
        Http &http;
        decltype(client) handle;
        ~Cleanup() {
            http.close(handle);
            http.cleanup(handle);
        }
    } cleanup{http, client};

    const auto header = [&](const char *name, const char *value) {
        return http.setHeader(client, name, value) == Http::OK;
    };
    if (!header("Content-Type", "application/json") ||
        !header("Accept-Encoding", "identity") ||
        !header("X-RAD-Firmware-Provenance-Capability", "1") ||
        (!identity.provenance.empty() &&
         (!header("X-RAD-Firmware-Provenance", identity.provenance.c_str()) ||
          !header("X-RAD-Firmware-Provenance-ID", identity.provenanceId.c_str()) ||
          !header("X-RAD-Firmware-Image-SHA256", identity.imageSha256.c_str())))) {
        error = "failed to set auth headers";
        return -2;
    }
    if (http.open(client, body.size()) != Http::OK) {
        error = "auth connection failed";
        return -1;
    }
    if (http.write(client, body.data(), body.size()) != static_cast<int>(body.size())) {
        error = "auth request write failed";
        return -3;
    }
    const std::int64_t contentLength = http.fetchHeaders(client);
    if (contentLength < 0) {
        error = "auth response headers failed";
        return -1;
    }
    const int status = http.statusCode(client);
    if (status != HTTP_OK) return status;
    if (contentLength > static_cast<std::int64_t>(MAX_RESPONSE_BYTES)) {
        error = "auth response is too large";
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    std::string responseBody;
    char buffer[512];
    int read = 0;
    while ((read = http.read(client, buffer, sizeof(buffer))) > 0) {
        if (responseBody.size() + static_cast<std::size_t>(read) > MAX_RESPONSE_BYTES) {
            error = "auth response is too large";
            return HTTP_INTERNAL_SERVER_ERROR;
        }
        responseBody.append(buffer, read);
    }
    if (read < 0) {
        error = "auth response read failed";
        return -5;
    }
    if ((contentLength > 0 && responseBody.size() != static_cast<std::size_t>(contentLength)) ||
        !http.isComplete(client)) {
        error = "auth response was incomplete";
        return HTTP_INTERNAL_SERVER_ERROR;
    }

    JsonDocument response;
    const auto jsonError = deserializeJson(response, responseBody);
    if (jsonError) {
        error = jsonError.c_str();
        return HTTP_INTERNAL_SERVER_ERROR;
    }
    // Preserve ArduinoJson's established conversions, including false for a
    // missing isPaired and the literal "null" for a missing pairingCode.
    paired = response["isPaired"].template as<bool>();
    if (updatePairingCode) {
        pairingCode = response["pairingCode"].template as<CodeString>();
    }
    return status;
}

}  // namespace pairing_auth
