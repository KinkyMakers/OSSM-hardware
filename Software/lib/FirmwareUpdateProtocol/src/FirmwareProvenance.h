#pragma once

#include "FirmwareUpdateProtocol.h"

#if defined(ARDUINO_ARCH_ESP32)

#include <Preferences.h>
#include <esp_ota_ops.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>

#include <array>
#include <cstring>
#include <vector>

namespace firmware::provenance {

constexpr const char *STAGING_KEY_ID = "rd-fw-staging-2026-08-01";
constexpr const char *PRODUCTION_KEY_ID = "rd-fw-production-2026-08-01";
constexpr const char *STAGING_PUBLIC_KEY = R"KEY(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEYb/VGPjyRATcddhxhnNs6IZYPSVM
7GKJFcCpnZgNEYj4z7N897PnOo/KMfyhE5xny9Kl5nJDxEDw0P12xkj75w==
-----END PUBLIC KEY-----
)KEY";
constexpr const char *PRODUCTION_PUBLIC_KEY = R"KEY(-----BEGIN PUBLIC KEY-----
MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEaXWHINx1VmUw0eMf5xCX5T5w24IZ
9K31OGTOy/s37oE7lbPyJanjrVq2u4DOqGLWc6YgSTwekFKO8VXq+GW9Cw==
-----END PUBLIC KEY-----
)KEY";

struct Claims {
    std::string keyId;
    std::string track;
    std::string deviceType;
    std::string version;
    std::string buildSha;
    std::string kind;
    std::string applicationSha256;
    std::uint32_t applicationSizeBytes = 0;
    std::string runtimeImageSha256;
};

struct Snapshot {
    std::string origin = "community";
    std::string token;
    std::string provenanceId;
    std::string keyId;
    std::string imageSha256;
    std::string buildSha;
};

inline bool provenanceBuildIdentifiersMatch(const std::string &left,
                                            const std::string &right) {
    if (left.size() < 7 || right.size() < 7 || left.size() > 64 ||
        right.size() > 64) {
        return false;
    }
    const std::size_t sharedLength = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < sharedLength; ++index) {
        const auto leftCharacter = static_cast<unsigned char>(left[index]);
        const auto rightCharacter = static_cast<unsigned char>(right[index]);
        if (!std::isxdigit(leftCharacter) || !std::isxdigit(rightCharacter) ||
            std::tolower(leftCharacter) != std::tolower(rightCharacter)) {
            return false;
        }
    }
    return true;
}

inline int base64Value(char value) {
    if (value >= 'A' && value <= 'Z') return value - 'A';
    if (value >= 'a' && value <= 'z') return value - 'a' + 26;
    if (value >= '0' && value <= '9') return value - '0' + 52;
    if (value == '-' || value == '+') return 62;
    if (value == '_' || value == '/') return 63;
    return -1;
}

inline bool decodeBase64Url(const std::string &input,
                            std::vector<unsigned char> &output) {
    output.clear();
    unsigned int buffer = 0;
    int bits = 0;
    for (char character : input) {
        if (character == '=') break;
        const int value = base64Value(character);
        if (value < 0) return false;
        buffer = (buffer << 6) | static_cast<unsigned int>(value);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            output.push_back(static_cast<unsigned char>((buffer >> bits) & 0xff));
        }
    }
    return !output.empty();
}

inline void appendDerInteger(std::vector<unsigned char> &output,
                             const unsigned char *value) {
    std::size_t start = 0;
    while (start < 31 && value[start] == 0) ++start;
    const bool prefixZero = (value[start] & 0x80) != 0;
    output.push_back(0x02);
    output.push_back(static_cast<unsigned char>(32 - start + (prefixZero ? 1 : 0)));
    if (prefixZero) output.push_back(0x00);
    output.insert(output.end(), value + start, value + 32);
}

inline bool p1363ToDer(const std::vector<unsigned char> &signature,
                       std::vector<unsigned char> &der) {
    if (signature.size() != 64) return false;
    std::vector<unsigned char> body;
    appendDerInteger(body, signature.data());
    appendDerInteger(body, signature.data() + 32);
    der = {0x30, static_cast<unsigned char>(body.size())};
    der.insert(der.end(), body.begin(), body.end());
    return true;
}

inline std::string tokenId(const std::string &token) {
    unsigned char digest[32] = {};
    mbedtls_sha256_ret(reinterpret_cast<const unsigned char *>(token.data()),
                       token.size(), digest, 0);
    std::vector<unsigned char> value(digest, digest + sizeof(digest));
    static constexpr char ALPHABET[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    std::string output;
    unsigned int buffer = 0;
    int bits = 0;
    for (unsigned char byte : value) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            output.push_back(ALPHABET[(buffer >> bits) & 0x3f]);
        }
    }
    if (bits > 0) output.push_back(ALPHABET[(buffer << (6 - bits)) & 0x3f]);
    return output;
}

inline bool verifyToken(const std::string &token, const DeviceReport &expected,
                        Claims &claims) {
    const auto first = token.find('.');
    const auto second = first == std::string::npos
                            ? std::string::npos
                            : token.find('.', first + 1);
    if (first == std::string::npos || second == std::string::npos ||
        token.find('.', second + 1) != std::string::npos) {
        return false;
    }
    std::vector<unsigned char> headerBytes, payloadBytes, signature, der;
    if (!decodeBase64Url(token.substr(0, first), headerBytes) ||
        !decodeBase64Url(token.substr(first + 1, second - first - 1), payloadBytes) ||
        !decodeBase64Url(token.substr(second + 1), signature) ||
        !p1363ToDer(signature, der)) {
        return false;
    }
    JsonDocument header;
    JsonDocument payload;
    if (deserializeJson(header, headerBytes.data(), headerBytes.size()) ||
        deserializeJson(payload, payloadBytes.data(), payloadBytes.size())) {
        return false;
    }
    const char *alg = header["alg"] | "";
    const char *typ = header["typ"] | "";
    const char *kid = header["kid"] | "";
    if (std::strcmp(alg, "ES256") != 0 ||
        std::strcmp(typ, "rad-fw-prov+jws") != 0) {
        return false;
    }
    const char *publicKey = nullptr;
    const char *keyTrack = nullptr;
    if (std::strcmp(kid, STAGING_KEY_ID) == 0) {
        publicKey = STAGING_PUBLIC_KEY;
        keyTrack = "staging";
    } else if (std::strcmp(kid, PRODUCTION_KEY_ID) == 0) {
        publicKey = PRODUCTION_PUBLIC_KEY;
        keyTrack = "main";
    } else {
        return false;
    }
    const std::string signingInput = token.substr(0, second);
    unsigned char digest[32] = {};
    if (mbedtls_sha256_ret(
            reinterpret_cast<const unsigned char *>(signingInput.data()),
            signingInput.size(), digest, 0) != 0) {
        return false;
    }
    mbedtls_pk_context key;
    mbedtls_pk_init(&key);
    const int parseResult = mbedtls_pk_parse_public_key(
        &key, reinterpret_cast<const unsigned char *>(publicKey),
        std::strlen(publicKey) + 1);
    const int verifyResult =
        parseResult == 0
            ? mbedtls_pk_verify(&key, MBEDTLS_MD_SHA256, digest, sizeof(digest),
                                der.data(), der.size())
            : parseResult;
    mbedtls_pk_free(&key);
    if (verifyResult != 0) return false;

    const char *schema = payload["schema"] | "";
    const char *issuer = payload["issuer"] | "";
    claims.keyId = kid;
    claims.track = payload["track"] | "";
    claims.deviceType = payload["deviceType"] | "";
    claims.version = payload["version"] | "";
    claims.buildSha = payload["buildSha"] | "";
    claims.kind = payload["kind"] | "";
    claims.applicationSha256 = payload["applicationSha256"] | "";
    claims.applicationSizeBytes = payload["applicationSizeBytes"] | 0;
    claims.runtimeImageSha256 = payload["runtimeImageSha256"] | "";
    return std::strcmp(schema, "rad.firmware.provenance.v1") == 0 &&
           std::strcmp(issuer, "research-and-desire") == 0 &&
           claims.track == keyTrack && claims.track == expected.reportedTrack &&
           claims.deviceType == expected.deviceType &&
           claims.version == expected.currentVersion &&
           (expected.currentBuild.empty() ||
            provenanceBuildIdentifiersMatch(claims.buildSha,
                                            expected.currentBuild)) &&
           (expected.firmwareHash.empty() ||
            claims.runtimeImageSha256 == expected.firmwareHash);
}

inline std::string readStored(const char *key) {
    Preferences preferences;
    if (!preferences.begin("fw-prov", true)) return {};
    const String value = preferences.getString(key, "");
    preferences.end();
    return value.c_str();
}

inline std::string currentToken() { return readStored("current"); }
inline std::string currentTokenId() {
    const auto token = currentToken();
    return token.empty() ? std::string{} : tokenId(token);
}
inline std::string currentImageSha256() {
    const auto token = currentToken();
    const auto first = token.find('.');
    const auto second = first == std::string::npos ? std::string::npos
                                                    : token.find('.', first + 1);
    if (second == std::string::npos) return {};
    std::vector<unsigned char> payload;
    if (!decodeBase64Url(token.substr(first + 1, second - first - 1), payload)) return {};
    JsonDocument document;
    if (deserializeJson(document, payload.data(), payload.size())) return {};
    return std::string(document["runtimeImageSha256"] | "");
}

inline bool writeStored(const char *key, const std::string &value) {
    Preferences preferences;
    if (!preferences.begin("fw-prov", false)) return false;
    const bool stored = preferences.putString(key, value.c_str()) > 0;
    preferences.end();
    return stored;
}

inline Snapshot snapshot(const DeviceReport &running) {
    Snapshot result;
    result.imageSha256 = running.firmwareHash;
    result.token = readStored("current");
    Claims claims;
    if (result.token.empty()) return result;
    if (!verifyToken(result.token, running, claims)) {
        result.origin = "invalid";
        result.provenanceId = tokenId(result.token);
        return result;
    }
    result.origin = "official";
    result.provenanceId = tokenId(result.token);
    result.keyId = claims.keyId;
    result.imageSha256 = claims.runtimeImageSha256;
    result.buildSha = claims.buildSha;
    return result;
}

inline std::string runningImageSha256() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    unsigned char digest[32] = {};
    if (running == nullptr ||
        esp_partition_get_sha256(running, digest) != ESP_OK) {
        return {};
    }
    static constexpr char HEX_CHARACTERS[] = "0123456789abcdef";
    std::string output;
    output.reserve(sizeof(digest) * 2);
    for (const unsigned char byte : digest) {
        output.push_back(HEX_CHARACTERS[byte >> 4]);
        output.push_back(HEX_CHARACTERS[byte & 0x0f]);
    }
    return output;
}

inline Snapshot runningSnapshot(const char *track, const char *deviceType,
                                const char *version, const char *buildSha) {
    DeviceReport report;
    report.reportedTrack = track == nullptr ? "" : track;
    report.deviceType = deviceType == nullptr ? "" : deviceType;
    report.currentVersion = version == nullptr ? "" : version;
    report.currentBuild = buildSha == nullptr ? "" : buildSha;
    report.firmwareHash = runningImageSha256();
    return snapshot(report);
}

inline void reconcile(DeviceReport &running) {
    const std::string pending = readStored("pending");
    Claims pendingClaims;
    if (!pending.empty() && verifyToken(pending, running, pendingClaims)) {
        writeStored("current", pending);
        Preferences preferences;
        if (preferences.begin("fw-prov", false)) {
            preferences.remove("pending");
            preferences.end();
        }
    }
    const Snapshot current = snapshot(running);
    if (!current.token.empty()) running.firmwareProvenance = current.token;
}

inline void observeCurrent(const DeviceReport &running,
                           const Decision &decision) {
    if (decision.currentProvenance.empty()) return;
    Claims claims;
    if (verifyToken(decision.currentProvenance, running, claims)) {
        writeStored("current", decision.currentProvenance);
    }
}

inline void stageUpdate(const DeviceReport &running, const Decision &decision) {
    if (!decision.shouldUpdate || decision.provenance.empty()) return;
    DeviceReport target = running;
    target.reportedTrack = decision.assignedTrack;
    target.currentVersion = decision.nextHopVersion;
    target.currentBuild = decision.buildSha;
    std::string applicationSha256;
    std::uint32_t applicationSizeBytes = 0;
    for (std::size_t index = 0; index < decision.artifactCount; ++index) {
        if (decision.artifacts[index].role == "application") {
            applicationSha256 = decision.artifacts[index].sha256;
            applicationSizeBytes = decision.artifacts[index].sizeBytes;
            break;
        }
    }
    target.firmwareHash.clear();
    Claims claims;
    if (verifyToken(decision.provenance, target, claims) &&
        claims.kind == decision.kind &&
        claims.applicationSha256 == applicationSha256 &&
        claims.applicationSizeBytes == applicationSizeBytes) {
        writeStored("pending", decision.provenance);
    }
}

}  // namespace firmware::provenance

#endif
