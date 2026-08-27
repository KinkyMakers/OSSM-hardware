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
    int protocolVersion = PROTOCOL_VERSION;
    std::string deviceType;
    std::string deviceId;
    std::string reportedTrack;
    std::string currentVersion;
    std::string currentBuild;
    std::string firmwareHash;
    int provenanceCapability = 1;
    std::string firmwareProvenance;
    std::string chip;
    std::uint32_t chipRevision = 0;
    std::uint32_t chipCores = 0;
    std::string hardwareRevision;
    std::uint32_t flashSizeBytes = 0;
    std::uint32_t psramSizeBytes = 0;
    std::uint32_t otaSlotSizeBytes = 0;
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
    int protocolVersion = PROTOCOL_VERSION;
    bool shouldUpdate = false;
    std::string reason;
    std::string reportedTrack;
    std::string assignedTrack;
    bool trackChanged = false;
    std::string currentVersion;
    std::string targetVersion;
    std::string nextHopVersion;
    std::string releaseId;
    std::string buildSha;
    std::string firmwareOrigin;
    std::string currentProvenance;
    std::string provenance;
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

inline bool isBuildSha(const std::string &value) {
    return value.size() >= 7 && value.size() <= 64 &&
           std::all_of(value.begin(), value.end(), [](const char character) {
               return std::isxdigit(static_cast<unsigned char>(character));
           });
}

inline bool isDecisionReason(const std::string &value) {
    return value == "update-available" || value == "already-current" ||
           value == "no-target" || value == "updates-disabled" ||
           value == "release-paused" || value == "rollout-denied" ||
           value == "incompatible-device";
}

// Repository build IDs may be reported as either full or abbreviated Git SHAs.
// Treat either prefix as the same build and compare without case sensitivity.
inline bool isSameBuild(const std::string &left, const std::string &right) {
    if (!isBuildSha(left) || !isBuildSha(right)) return false;
    const std::size_t length = std::min(left.size(), right.size());
    for (std::size_t index = 0; index < length; ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) {
            return false;
        }
    }
    return true;
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
    document["protocolVersion"] = report.protocolVersion;
    document["deviceType"] = report.deviceType;
    document["deviceId"] = report.deviceId;
    document["reportedTrack"] = report.reportedTrack;
    document["currentVersion"] = report.currentVersion;
    if (!report.currentBuild.empty())
        document["currentBuild"] = report.currentBuild;
    if (!report.firmwareHash.empty())
        document["firmwareHash"] = report.firmwareHash;
    document["provenanceCapability"] = report.provenanceCapability;
    if (!report.firmwareProvenance.empty())
        document["firmwareProvenance"] = report.firmwareProvenance;
    if (!report.chip.empty()) document["chip"] = report.chip;
    document["chipRevision"] = report.chipRevision;
    if (report.chipCores > 0) document["chipCores"] = report.chipCores;
    if (!report.hardwareRevision.empty())
        document["hardwareRevision"] = report.hardwareRevision;
    if (report.flashSizeBytes > 0)
        document["flashSizeBytes"] = report.flashSizeBytes;
    document["psramSizeBytes"] = report.psramSizeBytes;
    if (report.otaSlotSizeBytes > 0)
        document["otaSlotSizeBytes"] = report.otaSlotSizeBytes;
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
    const JsonVariantConst protocolVersion = document["protocolVersion"];
    if (!protocolVersion.isNull()) {
        if (!protocolVersion.is<int>() ||
            protocolVersion.as<int>() != PROTOCOL_VERSION) {
            error = "unsupported protocol version";
            return false;
        }
        parsed.protocolVersion = protocolVersion.as<int>();
    }

    const bool hasShouldUpdate = document["shouldUpdate"].is<bool>();
    const bool hasUpdateAvailable = document["updateAvailable"].is<bool>();
    if ((!hasShouldUpdate && !hasUpdateAvailable) ||
        (hasShouldUpdate && hasUpdateAvailable &&
         document["shouldUpdate"].as<bool>() !=
             document["updateAvailable"].as<bool>())) {
        error = "missing or conflicting update decision";
        return false;
    }
    parsed.shouldUpdate = hasShouldUpdate
                              ? document["shouldUpdate"].as<bool>()
                              : document["updateAvailable"].as<bool>();

    const JsonVariantConst reason = document["reason"];
    const bool canonicalResponse = !protocolVersion.isNull() ||
                                   hasShouldUpdate || !reason.isNull();
    if (canonicalResponse) {
        if (!hasShouldUpdate || !readRequiredString(reason, parsed.reason) ||
            !isDecisionReason(parsed.reason) ||
            parsed.shouldUpdate != (parsed.reason == "update-available")) {
            error = "invalid canonical update reason";
            return false;
        }
    }

    if (!document["trackChanged"].is<bool>() ||
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

    parsed.trackChanged = document["trackChanged"].as<bool>();
    parsed.nextCheckSeconds = document["nextCheckSeconds"].as<std::uint32_t>();
    if (!document["firmwareOrigin"].isNull() &&
        !readRequiredString(document["firmwareOrigin"], parsed.firmwareOrigin)) {
        error = "invalid firmware origin";
        return false;
    }
    if (!document["currentProvenance"].isNull() &&
        !readRequiredString(document["currentProvenance"], parsed.currentProvenance)) {
        error = "invalid current firmware provenance";
        return false;
    }
    if (!parsed.shouldUpdate) {
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

    if (!update["buildSha"].isNull()) {
        if (!readRequiredString(update["buildSha"], parsed.buildSha) ||
            !isBuildSha(parsed.buildSha)) {
            error = "invalid update build SHA";
            return false;
        }
    } else if (canonicalResponse) {
        error = "canonical update response is missing build SHA";
        return false;
    }
    if (!update["provenance"].isNull() &&
        !readRequiredString(update["provenance"], parsed.provenance)) {
        error = "invalid update firmware provenance";
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

inline bool validateDecisionForReport(const DeviceReport &report,
                                      const Decision &decision,
                                      std::string &error) {
    if (decision.reportedTrack != report.reportedTrack ||
        decision.currentVersion != report.currentVersion ||
        decision.trackChanged !=
            (decision.reportedTrack != decision.assignedTrack)) {
        error = "firmware response does not match device report";
        return false;
    }
    if (decision.shouldUpdate &&
        decision.assignedTrack == report.reportedTrack) {
        if (isSameBuild(report.currentBuild, decision.buildSha)) {
            error = "firmware response selected the running build";
            return false;
        }
        // Legacy responses do not identify the build. On the same track, a
        // same-version offer cannot be distinguished from the running image.
        if (decision.buildSha.empty() &&
            decision.nextHopVersion == report.currentVersion) {
            error = "firmware response selected the running version";
            return false;
        }
    }
    return true;
}

}  // namespace firmware
