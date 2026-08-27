#include <unity.h>

#include <functional>

#include "FirmwareUpdateProtocol.h"

namespace {

const char *VALID_RESPONSE = R"json({
  "protocolVersion": 1,
  "shouldUpdate": true,
  "updateAvailable": true,
  "reason": "update-available",
  "reportedTrack": "main",
  "assignedTrack": "staging",
  "trackChanged": true,
  "firmwareOrigin": "official",
  "currentProvenance": "current.jws.token",
  "currentVersion": "1.0.34",
  "targetVersion": "1.0.35",
  "nextHopVersion": "1.0.35",
  "update": {
    "provenance": "target.jws.token",
    "releaseId": "00000000-0000-4000-8000-000000000001",
    "buildSha": "0123456789abcdef0123456789abcdef01234567",
    "kind": "firmware",
    "publishedAt": "2026-07-16T00:00:00.000Z",
    "artifacts": [
      {
        "role": "application",
        "url": "https://example.supabase.co/storage/v1/object/public/ossm-firmware/releases/1.0.35/0123456789abcdef/firmware.bin",
        "sha256": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "sizeBytes": 1024,
        "installOrder": 1
      }
    ]
  },
  "nextCheckSeconds": 60
})json";

std::string mutateResponse(
    const std::function<void(JsonDocument &)> &mutation) {
    JsonDocument document;
    deserializeJson(document, VALID_RESPONSE);
    mutation(document);
    std::string payload;
    serializeJson(document, payload);
    return payload;
}

firmware::DeviceReport matchingReport() {
    return {
        .deviceType = "ossm",
        .deviceId = "AA:BB:CC:DD:EE:FF",
        .reportedTrack = "main",
        .currentVersion = "1.0.34",
        .currentBuild = "fedcba9876543210",
    };
}

void test_serializes_version_identity_and_hardware_fields() {
    firmware::DeviceReport report{
        .deviceType = "ossm",
        .deviceId = "AA:BB:CC:DD:EE:FF",
        .reportedTrack = "main",
        .currentVersion = "1.0.34",
        .currentBuild = "0123456789abcdef",
        .firmwareHash =
            "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        .provenanceCapability = 1,
        .firmwareProvenance = "current.jws.token",
        .chip = "ESP32-D0WDQ6",
        .chipRevision = 0,
        .chipCores = 2,
        .hardwareRevision = "ossm-v1",
        .flashSizeBytes = 4194304,
        .psramSizeBytes = 0,
        .otaSlotSizeBytes = 1966080,
        .partitionLayout = "ossm-ota-v1",
    };
    JsonDocument parsed;
    deserializeJson(parsed, firmware::serializeReport(report));
    TEST_ASSERT_EQUAL_INT(1, parsed["protocolVersion"]);
    TEST_ASSERT_EQUAL_STRING("ossm", parsed["deviceType"]);
    TEST_ASSERT_EQUAL_STRING("AA:BB:CC:DD:EE:FF", parsed["deviceId"]);
    TEST_ASSERT_EQUAL_STRING("main", parsed["reportedTrack"]);
    TEST_ASSERT_EQUAL_INT(1, parsed["provenanceCapability"]);
    TEST_ASSERT_EQUAL_STRING("current.jws.token",
                             parsed["firmwareProvenance"]);
    TEST_ASSERT_EQUAL_STRING("1.0.34", parsed["currentVersion"]);
    TEST_ASSERT_EQUAL_STRING("0123456789abcdef", parsed["currentBuild"]);
    TEST_ASSERT_EQUAL_STRING(report.firmwareHash.c_str(), parsed["firmwareHash"]);
    TEST_ASSERT_EQUAL_UINT32(0, parsed["chipRevision"]);
    TEST_ASSERT_EQUAL_UINT32(2, parsed["chipCores"]);
    TEST_ASSERT_EQUAL_UINT32(4194304, parsed["flashSizeBytes"]);
    TEST_ASSERT_EQUAL_UINT32(0, parsed["psramSizeBytes"]);
    TEST_ASSERT_EQUAL_UINT32(1966080, parsed["otaSlotSizeBytes"]);
    TEST_ASSERT_EQUAL_STRING("ossm-ota-v1", parsed["partitionLayout"]);

    report.otaSlotSizeBytes = 0;
    report.chipCores = 0;
    JsonDocument withoutOtaSlot;
    deserializeJson(withoutOtaSlot, firmware::serializeReport(report));
    TEST_ASSERT_TRUE(withoutOtaSlot["otaSlotSizeBytes"].isNull());
    TEST_ASSERT_TRUE(withoutOtaSlot["chipCores"].isNull());
    TEST_ASSERT_EQUAL_UINT32(0, withoutOtaSlot["chipRevision"]);
    TEST_ASSERT_EQUAL_UINT32(0, withoutOtaSlot["psramSizeBytes"]);
}

void test_parses_canonical_decision_and_orders_artifacts() {
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_TRUE(
        firmware::parseDecision(VALID_RESPONSE, "ossm", decision, error));
    TEST_ASSERT_TRUE(decision.shouldUpdate);
    TEST_ASSERT_EQUAL_INT(1, decision.protocolVersion);
    TEST_ASSERT_EQUAL_STRING("update-available", decision.reason.c_str());
    TEST_ASSERT_EQUAL_STRING("staging", decision.assignedTrack.c_str());
    TEST_ASSERT_EQUAL_STRING("official", decision.firmwareOrigin.c_str());
    TEST_ASSERT_EQUAL_STRING("current.jws.token",
                             decision.currentProvenance.c_str());
    TEST_ASSERT_EQUAL_STRING("target.jws.token",
                             decision.provenance.c_str());
    TEST_ASSERT_TRUE(decision.trackChanged);
    TEST_ASSERT_EQUAL_STRING("1.0.35", decision.nextHopVersion.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "0123456789abcdef0123456789abcdef01234567",
        decision.buildSha.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, decision.artifactCount);
    TEST_ASSERT_EQUAL_STRING("application", decision.artifacts[0].role.c_str());
}

void test_accepts_legacy_update_available_alias() {
    const std::string payload = mutateResponse([](JsonDocument &document) {
        document.remove("protocolVersion");
        document.remove("shouldUpdate");
        document.remove("reason");
        document["update"].as<JsonObject>().remove("buildSha");
    });
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_TRUE(
        firmware::parseDecision(payload, "ossm", decision, error));
    TEST_ASSERT_TRUE(decision.shouldUpdate);
    TEST_ASSERT_TRUE(decision.buildSha.empty());
}

void test_rejects_conflicting_decision_booleans_and_protocols() {
    firmware::Decision decision;
    std::string error;
    std::string payload = mutateResponse(
        [](JsonDocument &document) { document["updateAvailable"] = false; });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = mutateResponse(
        [](JsonDocument &document) { document["protocolVersion"] = 2; });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));
}

void test_validates_canonical_reason_decision_pairing() {
    firmware::Decision decision;
    std::string error;
    std::string payload = mutateResponse([](JsonDocument &document) {
        document["reason"] = "already-current";
    });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = mutateResponse([](JsonDocument &document) {
        document["reason"] = "unexpected-reason";
    });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = mutateResponse([](JsonDocument &document) {
        document["shouldUpdate"] = false;
        document["updateAvailable"] = false;
        document["reason"] = "incompatible-device";
        document["targetVersion"] = nullptr;
        document["nextHopVersion"] = nullptr;
        document["update"] = nullptr;
    });
    TEST_ASSERT_TRUE(
        firmware::parseDecision(payload, "ossm", decision, error));
    TEST_ASSERT_FALSE(decision.shouldUpdate);
    TEST_ASSERT_EQUAL_STRING("incompatible-device", decision.reason.c_str());
}

void test_rejects_running_build_with_case_or_abbreviated_sha() {
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_TRUE(
        firmware::parseDecision(VALID_RESPONSE, "ossm", decision, error));

    firmware::DeviceReport report = matchingReport();
    report.currentBuild = "0123456789ABCDEF";
    // Cross-track rebuilds are intentional because the assigned track is
    // compiled into the firmware, even when source SHA and version match.
    TEST_ASSERT_TRUE(
        firmware::validateDecisionForReport(report, decision, error));

    decision.assignedTrack = report.reportedTrack;
    decision.trackChanged = false;
    TEST_ASSERT_FALSE(
        firmware::validateDecisionForReport(report, decision, error));

    report.currentBuild = "fedcba9876543210";
    TEST_ASSERT_TRUE(
        firmware::validateDecisionForReport(report, decision, error));

    decision.nextHopVersion = report.currentVersion;
    TEST_ASSERT_TRUE(
        firmware::validateDecisionForReport(report, decision, error));

    decision.buildSha.clear();
    TEST_ASSERT_FALSE(
        firmware::validateDecisionForReport(report, decision, error));

    decision.assignedTrack = "staging";
    decision.trackChanged = true;
    TEST_ASSERT_TRUE(
        firmware::validateDecisionForReport(report, decision, error));
}

void test_rejects_mismatched_report_identity() {
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_TRUE(
        firmware::parseDecision(VALID_RESPONSE, "ossm", decision, error));
    firmware::DeviceReport report = matchingReport();
    report.currentVersion = "1.0.33";
    TEST_ASSERT_FALSE(
        firmware::validateDecisionForReport(report, decision, error));
}

void test_rejects_wrong_bucket_and_non_https_urls() {
    std::string payload = VALID_RESPONSE;
    const auto bucket = payload.find("ossm-firmware");
    payload.replace(bucket, std::string("ossm-firmware").size(), "lkbx-firmware");
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = VALID_RESPONSE;
    const auto scheme = payload.find("https://");
    payload.replace(scheme, std::string("https://").size(), "http://");
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = VALID_RESPONSE;
    const auto trustedHost = payload.find("example.supabase.co");
    payload.replace(trustedHost, std::string("example.supabase.co").size(),
                    "evil.example");
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));
}

void test_rejects_bad_hash_build_sha_and_missing_fields() {
    std::string payload = VALID_RESPONSE;
    const auto hash = payload.find(std::string(64, 'a'));
    payload.replace(hash, 64, "bad");
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = mutateResponse([](JsonDocument &document) {
        document["update"]["buildSha"] = "not-a-git-sha";
    });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));

    payload = mutateResponse([](JsonDocument &document) {
        document["update"].as<JsonObject>().remove("buildSha");
    });
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));
    TEST_ASSERT_FALSE(firmware::parseDecision("{}", "ossm", decision, error));
}

}  // namespace

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_serializes_version_identity_and_hardware_fields);
    RUN_TEST(test_parses_canonical_decision_and_orders_artifacts);
    RUN_TEST(test_accepts_legacy_update_available_alias);
    RUN_TEST(test_rejects_conflicting_decision_booleans_and_protocols);
    RUN_TEST(test_validates_canonical_reason_decision_pairing);
    RUN_TEST(test_rejects_running_build_with_case_or_abbreviated_sha);
    RUN_TEST(test_rejects_mismatched_report_identity);
    RUN_TEST(test_rejects_wrong_bucket_and_non_https_urls);
    RUN_TEST(test_rejects_bad_hash_build_sha_and_missing_fields);
    return UNITY_END();
}
