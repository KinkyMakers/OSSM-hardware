#include <unity.h>

#include "FirmwareUpdateProtocol.h"

namespace {

const char *VALID_RESPONSE = R"json({
  "updateAvailable": true,
  "reportedTrack": "main",
  "assignedTrack": "staging",
  "trackChanged": true,
  "currentVersion": "1.0.34",
  "targetVersion": "1.0.35",
  "nextHopVersion": "1.0.35",
  "update": {
    "releaseId": "00000000-0000-4000-8000-000000000001",
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

void test_serializes_required_and_hardware_fields() {
    firmware::DeviceReport report{
        .deviceType = "ossm",
        .deviceId = "trainer-test-id",
        .reportedTrack = "main",
        .currentVersion = "1.0.34",
        .currentBuild = "0123456789abcdef",
        .chip = "esp32",
        .flashSizeBytes = 4194304,
        .partitionLayout = "legacy-v1",
    };
    JsonDocument parsed;
    deserializeJson(parsed, firmware::serializeReport(report));
    TEST_ASSERT_EQUAL_STRING("ossm", parsed["deviceType"]);
    TEST_ASSERT_EQUAL_STRING("main", parsed["reportedTrack"]);
    TEST_ASSERT_EQUAL_UINT32(4194304, parsed["flashSizeBytes"]);
    TEST_ASSERT_EQUAL_STRING("legacy-v1", parsed["partitionLayout"]);
}

void test_parses_cross_track_update_and_orders_artifacts() {
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_TRUE(
        firmware::parseDecision(VALID_RESPONSE, "ossm", decision, error));
    TEST_ASSERT_TRUE(decision.updateAvailable);
    TEST_ASSERT_EQUAL_STRING("staging", decision.assignedTrack.c_str());
    TEST_ASSERT_TRUE(decision.trackChanged);
    TEST_ASSERT_EQUAL_STRING("1.0.35", decision.nextHopVersion.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, decision.artifactCount);
    TEST_ASSERT_EQUAL_STRING("application", decision.artifacts[0].role.c_str());
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

void test_rejects_bad_hash_duplicate_order_and_missing_fields() {
    std::string payload = VALID_RESPONSE;
    const auto hash = payload.find(std::string(64, 'a'));
    payload.replace(hash, 64, "bad");
    firmware::Decision decision;
    std::string error;
    TEST_ASSERT_FALSE(
        firmware::parseDecision(payload, "ossm", decision, error));
    TEST_ASSERT_FALSE(firmware::parseDecision("{}", "ossm", decision, error));
}

}  // namespace

int main(int, char **) {
    UNITY_BEGIN();
    RUN_TEST(test_serializes_required_and_hardware_fields);
    RUN_TEST(test_parses_cross_track_update_and_orders_artifacts);
    RUN_TEST(test_rejects_wrong_bucket_and_non_https_urls);
    RUN_TEST(test_rejects_bad_hash_duplicate_order_and_missing_fields);
    return UNITY_END();
}
