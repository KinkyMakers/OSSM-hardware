// Exercise the production serializer shared by MQTT and full BLE state reads.
// The dashboard schema lives in RAD App at:
// packages/supabase/functions/ossm-mqtt-telemetry/index.ts
// Its proxy adds the outer {payload: ...} wrapper. Buffer is a BLE field that
// the dashboard ignores; provenance identifies the running firmware.

#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>
#include <limits>

#include "telemetry_payload.h"
#include <set>
#include <string>

// --- Required keys that the Dashboard Zod schema mandates ---
static const std::set<std::string> REQUIRED_KEYS = {
    "timestamp", "state", "speed",     "stroke", "sensation",
    "depth",     "pattern", "position", "sessionId",
};

// Preserve the two additional fields already emitted by the firmware.
static const std::set<std::string> OPTIONAL_KEYS = {
    "meta", "buffer", "firmwareProvenanceId"
};

// Parse the real wire payload; no manually maintained parallel serializer.
static JsonDocument buildPayload(unsigned long timestamp,
                                 const char* state, float speed, float stroke,
                                 float sensation, float depth, int pattern,
                                 float position, const char* sessionId,
                                 float buffer = 100,
                                 const char* provenanceId = "") {
    const String payload = telemetry::serialize({timestamp, state, speed, stroke,
        sensation, depth, buffer, pattern, position, sessionId, provenanceId});
    JsonDocument doc;
    const auto error = deserializeJson(doc, payload.c_str());
    TEST_ASSERT_EQUAL_MESSAGE(DeserializationError::Ok, error.code(),
                               "Production telemetry must be valid JSON");
    return doc;
}

// ─── Test: all required keys are present ─────────────────────────────────

void test_payload_has_all_required_keys() {
    auto doc = buildPayload(12345, "strokeEngine", 50, 80, 66, 67, 2, 118.05f,
                            "d3325d48-2675-4b44-99fe-6d722568f29e");

    for (const auto& key : REQUIRED_KEYS) {
        TEST_ASSERT_TRUE_MESSAGE(!doc[key].isNull(),
                                 (std::string("Missing required key: ") + key).c_str());
    }
}

// ─── Test: no unexpected keys ────────────────────────────────────────────
// Zod's default mode strips unknown keys, but extra keys waste bandwidth
// and signal a contract drift.

void test_payload_has_no_extra_keys() {
    auto doc = buildPayload(12345, "menu.idle", 32, 2, 66, 67, 2, 118.05f,
                            "d3325d48-2675-4b44-99fe-6d722568f29e");

    for (JsonPair kv : doc.as<JsonObject>()) {
        std::string key = kv.key().c_str();
        bool isKnown = REQUIRED_KEYS.count(key) || OPTIONAL_KEYS.count(key);
        TEST_ASSERT_TRUE_MESSAGE(isKnown,
                                 (std::string("Unexpected key in payload: ") + key).c_str());
    }
}

// ─── Test: types match Zod expectations ──────────────────────────────────

void test_timestamp_is_number() {
    auto doc = buildPayload(99999, "idle", 0, 0, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["timestamp"].is<unsigned long>());
}

void test_state_is_string() {
    auto doc = buildPayload(0, "strokeEngine.pattern", 0, 0, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["state"].is<const char*>());
}

void test_speed_is_integer() {
    auto doc = buildPayload(0, "s", 42, 0, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["speed"].is<int>());
    TEST_ASSERT_EQUAL_INT(42, doc["speed"].as<int>());
}

void test_stroke_is_integer() {
    auto doc = buildPayload(0, "s", 0, 80, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["stroke"].is<int>());
}

void test_sensation_is_integer() {
    auto doc = buildPayload(0, "s", 0, 0, 55, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["sensation"].is<int>());
}

void test_depth_is_integer() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 33, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["depth"].is<int>());
}

void test_pattern_is_integer() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 0, 6, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["pattern"].is<int>());
    TEST_ASSERT_EQUAL_INT(6, doc["pattern"].as<int>());
}

void test_position_is_float() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 0, 0, 123.456f,
                            "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_TRUE(doc["position"].is<float>());
}

void test_sessionId_is_string() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 0, 0, 0.0f,
                            "d3325d48-2675-4b44-99fe-6d722568f29e");
    TEST_ASSERT_TRUE(doc["sessionId"].is<const char*>());
    TEST_ASSERT_EQUAL_STRING("d3325d48-2675-4b44-99fe-6d722568f29e",
                             doc["sessionId"].as<const char*>());
}

// ─── Test: serialized JSON round-trips correctly ─────────────────────────

void test_serialized_json_round_trips() {
    auto doc = buildPayload(5000, "strokeEngine", 75, 90, 50, 40, 3, 55.5f,
                            "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");

    std::string json;
    serializeJson(doc, json);

    JsonDocument parsed;
    DeserializationError err = deserializeJson(parsed, json);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    TEST_ASSERT_EQUAL(5000, parsed["timestamp"].as<unsigned long>());
    TEST_ASSERT_EQUAL_STRING("strokeEngine", parsed["state"]);
    TEST_ASSERT_EQUAL_INT(75, parsed["speed"]);
    TEST_ASSERT_EQUAL_INT(90, parsed["stroke"]);
    TEST_ASSERT_EQUAL_INT(50, parsed["sensation"]);
    TEST_ASSERT_EQUAL_INT(40, parsed["depth"]);
    TEST_ASSERT_EQUAL_INT(3, parsed["pattern"]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 55.5f, parsed["position"].as<float>());
    TEST_ASSERT_EQUAL_STRING("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
                             parsed["sessionId"]);
}

// ─── Test: all 11 existing firmware fields are emitted without meta ────

void test_key_count_without_meta() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    int count = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        (void)kv;
        count++;
    }
    TEST_ASSERT_EQUAL_INT(11, count);
}

// ─── Test: pattern enum values stay within known range ───────────────────
// StrokePatterns has 7 values (0-6). The Dashboard stores this as an int
// column and doesn't validate the range, but we should stay sane.

void test_pattern_boundary_values() {
    for (int p = 0; p <= 6; p++) {
        auto doc = buildPayload(0, "s", 0, 0, 0, 0, p, 0.0f,
                                "00000000-0000-0000-0000-000000000000");
        TEST_ASSERT_EQUAL_INT(p, doc["pattern"].as<int>());
    }
}

void test_full_state_keeps_buffer_and_firmware_provenance() {
    const char* provenance = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopq";
    auto doc = buildPayload(0, "error.idle", 0, 50, 50, 10, 0, 0.0f,
        "00000000-0000-0000-0000-000000000000", 75, provenance);
    TEST_ASSERT_EQUAL_INT(75, doc["buffer"].as<int>());
    TEST_ASSERT_EQUAL_STRING(provenance, doc["firmwareProvenanceId"]);
}

void test_wire_format_preserves_percent_truncation_and_position_rounding() {
    const String actual = telemetry::serialize({4294967295UL, "strokeEngine",
        50.9f, 80.1f, 40.8f, 60.4f, 100.9f, 6, 12.346f,
        "00000000-0000-0000-0000-000000000000", ""});
    TEST_ASSERT_EQUAL_STRING(
        "{\"timestamp\":4294967295,\"state\":\"strokeEngine\","
        "\"speed\":50,\"stroke\":80,\"sensation\":40,\"depth\":60,"
        "\"buffer\":100,\"pattern\":6,\"position\":12.35,"
        "\"sessionId\":\"00000000-0000-0000-0000-000000000000\","
        "\"firmwareProvenanceId\":\"\"}", actual.c_str());
}

void test_nonfinite_position_keeps_existing_nan_fallback() {
    auto doc = buildPayload(0, "menu.idle", 0, 0, 0, 0, 0,
        std::numeric_limits<float>::quiet_NaN(),
        "00000000-0000-0000-0000-000000000000");
    TEST_ASSERT_EQUAL_FLOAT(0.0f, doc["position"].as<float>());
}

// ─── Runner ──────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_full_state_keeps_buffer_and_firmware_provenance);
    RUN_TEST(test_wire_format_preserves_percent_truncation_and_position_rounding);
    RUN_TEST(test_nonfinite_position_keeps_existing_nan_fallback);

    RUN_TEST(test_payload_has_all_required_keys);
    RUN_TEST(test_payload_has_no_extra_keys);
    RUN_TEST(test_timestamp_is_number);
    RUN_TEST(test_state_is_string);
    RUN_TEST(test_speed_is_integer);
    RUN_TEST(test_stroke_is_integer);
    RUN_TEST(test_sensation_is_integer);
    RUN_TEST(test_depth_is_integer);
    RUN_TEST(test_pattern_is_integer);
    RUN_TEST(test_position_is_float);
    RUN_TEST(test_sessionId_is_string);
    RUN_TEST(test_serialized_json_round_trips);
    RUN_TEST(test_key_count_without_meta);
    RUN_TEST(test_pattern_boundary_values);

    return UNITY_END();
}
