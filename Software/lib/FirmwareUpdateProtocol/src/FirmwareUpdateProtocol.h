#pragma once

#include <ArduinoJson.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>

namespace firmware {

constexpr int PROTOCOL_VERSION = 1;
constexpr std::size_t MAX_ARTIFACTS = 8;

struct DeviceReport {
    std::string deviceType;
    std::string deviceId;
    std::string reportedTrack;
    std::string currentVersion;
    std::string currentBuild;
    std::string firmwareHash;
    std::string chip;
    std::string hardwareRevision;
    std::uint32_t flashSizeBytes = 0;
    std::string partitionLayout;
};

struct Artifact {
    std::string role;
    std::string url;
    std::string sha256;
    std::uint32_t sizeBytes = 0;
    int installOrder = 0;
};

struct Decision {
    bool updateAvailable = false;
    std::string reportedTrack;
    std::string assignedTrack;
    bool trackChanged = false;
    std::string currentVersion;
    std::string targetVersion;
    std::string nextHopVersion;
    std::string releaseId;
    std::string kind;
    std::array<Artifact, MAX_ARTIFACTS> artifacts{};
    std::size_t artifactCount = 0;
    std::uint32_t nextCheckSeconds = 0;
};

inline bool isTrack(const std::string &value) {
    return value == "main" || value == "staging";
}

inline bool isSemver(const std::string &value) {
    int dots = 0;
    bool digitInPart = false;
    for (const char character : value) {
        if (character == '.') {
            if (!digitInPart || ++dots > 2) return false;
            digitInPart = false;
        } else if (character == '+' || character == '-') {
            break;
        } else if (std::isdigit(static_cast<unsigned char>(character))) {
            digitInPart = true;
        } else {
            return false;
        }
    }
    return dots == 2 && digitInPart;
}

inline bool isSha256(const std::string &value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](const char character) {
               return std::isxdigit(static_cast<unsigned char>(character));
           });
}

inline bool isArtifactRole(const std::string &role) {
    return role == "application" || role == "filesystem" ||
           role == "bootloader" || role == "partitions" ||
           role == "manifest";
}

inline bool isHttpsArtifactUrl(const std::string &url,
                               const std::string &expectedBucket) {
    const std::string prefix = "https://";
    const std::string hostSuffix = ".supabase.co";
    const std::string storagePath =
        "/storage/v1/object/public/" + expectedBucket +
        "/releases/";
    if (url.rfind(prefix, 0) != 0 || url.find('@') != std::string::npos ||
        url.find('#') != std::string::npos || url.find('?') != std::string::npos) {
        return false;
    }
    const auto hostEnd = url.find('/', prefix.size());
    if (hostEnd == std::string::npos) return false;
    const std::string host = url.substr(prefix.size(), hostEnd - prefix.size());
    if (host.size() <= hostSuffix.size() ||
        host.compare(host.size() - hostSuffix.size(), hostSuffix.size(),
                     hostSuffix) != 0 ||
        host.find(':') != std::string::npos) {
        return false;
    }
    return url.compare(hostEnd, storagePath.size(), storagePath) == 0;
}

inline std::string serializeReport(const DeviceReport &report) {
    JsonDocument document;
    document["deviceType"] = report.deviceType;
    document["deviceId"] = report.deviceId;
    document["reportedTrack"] = report.reportedTrack;
    document["currentVersion"] = report.currentVersion;
    if (!report.currentBuild.empty())
        document["currentBuild"] = report.currentBuild;
    if (!report.firmwareHash.empty())
        document["firmwareHash"] = report.firmwareHash;
    if (!report.chip.empty()) document["chip"] = report.chip;
    if (!report.hardwareRevision.empty())
        document["hardwareRevision"] = report.hardwareRevision;
    if (report.flashSizeBytes > 0)
        document["flashSizeBytes"] = report.flashSizeBytes;
    if (!report.partitionLayout.empty())
        document["partitionLayout"] = report.partitionLayout;
    std::string output;
    serializeJson(document, output);
    return output;
}

inline bool readRequiredString(JsonVariantConst value, std::string &output) {
    if (!value.is<const char *>()) return false;
    output = value.as<const char *>();
    return !output.empty();
}

inline bool readNullableVersion(JsonVariantConst value, std::string &output) {
    if (value.isNull()) {
        output.clear();
        return true;
    }
    return readRequiredString(value, output) && isSemver(output);
}

inline bool parseDecision(const std::string &payload,
                          const std::string &expectedDeviceType,
                          Decision &decision, std::string &error) {
    JsonDocument document;
    const auto jsonError = deserializeJson(document, payload);
    if (jsonError) {
        error = "invalid JSON";
        return false;
    }

    Decision parsed;
    if (!document["updateAvailable"].is<bool>() ||
        !document["trackChanged"].is<bool>() ||
        !document["nextCheckSeconds"].is<std::uint32_t>() ||
        !readRequiredString(document["reportedTrack"], parsed.reportedTrack) ||
        !readRequiredString(document["assignedTrack"], parsed.assignedTrack) ||
        !readRequiredString(document["currentVersion"], parsed.currentVersion) ||
        !readNullableVersion(document["targetVersion"], parsed.targetVersion) ||
        !readNullableVersion(document["nextHopVersion"], parsed.nextHopVersion) ||
        !isTrack(parsed.reportedTrack) || !isTrack(parsed.assignedTrack) ||
        !isSemver(parsed.currentVersion)) {
        error = "missing or invalid response fields";
        return false;
    }

    parsed.updateAvailable = document["updateAvailable"].as<bool>();
    parsed.trackChanged = document["trackChanged"].as<bool>();
    parsed.nextCheckSeconds = document["nextCheckSeconds"].as<std::uint32_t>();
    if (!parsed.updateAvailable) {
        if (!document["update"].isNull()) {
            error = "no-update response contains update data";
            return false;
        }
        decision = parsed;
        return true;
    }

    JsonObjectConst update = document["update"].as<JsonObjectConst>();
    JsonArrayConst artifacts = update["artifacts"].as<JsonArrayConst>();
    if (update.isNull() || artifacts.isNull() || artifacts.size() == 0 ||
        artifacts.size() > MAX_ARTIFACTS ||
        !readRequiredString(update["releaseId"], parsed.releaseId) ||
        !readRequiredString(update["kind"], parsed.kind) ||
        (parsed.kind != "firmware" && parsed.kind != "migration") ||
        parsed.nextHopVersion.empty()) {
        error = "invalid update metadata";
        return false;
    }

    const std::string expectedBucket = expectedDeviceType + "-firmware";
    std::array<bool, MAX_ARTIFACTS> seenOrders{};
    for (JsonObjectConst item : artifacts) {
        Artifact artifact;
        if (!readRequiredString(item["role"], artifact.role) ||
            !readRequiredString(item["url"], artifact.url) ||
            !readRequiredString(item["sha256"], artifact.sha256) ||
            !item["sizeBytes"].is<std::uint32_t>() ||
            !item["installOrder"].is<int>() ||
            !isArtifactRole(artifact.role) || !isSha256(artifact.sha256) ||
            !isHttpsArtifactUrl(artifact.url, expectedBucket)) {
            error = "invalid artifact";
            return false;
        }
        artifact.sizeBytes = item["sizeBytes"].as<std::uint32_t>();
        artifact.installOrder = item["installOrder"].as<int>();
        if (artifact.sizeBytes == 0 || artifact.installOrder < 0 ||
            artifact.installOrder >= static_cast<int>(MAX_ARTIFACTS) ||
            seenOrders[artifact.installOrder]) {
            error = "invalid artifact size or order";
            return false;
        }
        seenOrders[artifact.installOrder] = true;
        parsed.artifacts[parsed.artifactCount++] = artifact;
    }

    std::sort(parsed.artifacts.begin(),
              parsed.artifacts.begin() + parsed.artifactCount,
              [](const Artifact &left, const Artifact &right) {
                  return left.installOrder < right.installOrder;
              });
    decision = parsed;
    return true;
}

}  // namespace firmware
