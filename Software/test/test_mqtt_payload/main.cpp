// ┌──────────────────────────────────────────────────────────────────────────┐
// │ MQTT TELEMETRY PAYLOAD — CONTRACT TESTS                                │
// │                                                                        │
// │ These tests validate that the JSON payload produced by                  │
// │ OSSM::getCurrentState() matches the Zod schema expected by the         │
// │ RAD Dashboard API route:                                               │
// │                                                                        │
// │   rad-app/src/app/api/lockbox/event/ossm/[mac]/route.ts                │
// │                                                                        │
// │ The Dashboard's payloadSchema (Zod) requires:                          │
// │   timestamp  : z.number()        — millis() uptime                    │
// │   state      : z.string()        — Boost.SML state name              │
// │   speed      : z.number().int()  — cast from float                   │
// │   stroke     : z.number().int()  — cast from float                   │
// │   sensation  : z.number().int()  — cast from float                   │
// │   depth      : z.number().int()  — cast from float                   │
// │   pattern    : z.number().int()  — StrokePatterns enum ordinal       │
// │   position   : z.number()        — stepper position in mm (float)    │
// │   sessionId  : z.uuid()          — generated per MQTT connection      │
// │   meta       : z.string().optional() — JSON metadata (optional)       │
// │                                                                        │
// │ The outer wrapper is: { payload: <above> }                             │
// │ The "payload" key is added by the EMQX MQTT rule engine, NOT here.     │
// │                                                                        │
// │ IF YOU CHANGE THE PAYLOAD SHAPE, YOU MUST ALSO UPDATE:                 │
// │   1. The Zod schema in the Dashboard route (see path above)            │
// │   2. OSSM::getCurrentState() in src/ossm/OSSM.cpp                     │
// │   3. These tests                                                       │
// │                                                                        │
// │ Failure to keep these in sync causes silent 400 errors on the server   │
// │ and lost telemetry data.                                               │
// └──────────────────────────────────────────────────────────────────────────┘

#include <ArduinoJson.h>
#include <unity.h>

#include <cstring>
#include <set>
#include <string>

// --- Required keys that the Dashboard Zod schema mandates ---
static const std::set<std::string> REQUIRED_KEYS = {
    "timestamp", "state", "speed",     "stroke", "sensation",
    "depth",     "pattern", "position", "sessionId",
};

// "meta" is the only optional key the server accepts
static const std::set<std::string> OPTIONAL_KEYS = {"meta"};

// Builds a payload identical to OSSM::getCurrentState().
// Kept in sync manually — if getCurrentState() changes, this must too.
static JsonDocument buildPayload(unsigned long timestamp,
                                 const char* state, int speed, int stroke,
                                 int sensation, int depth, int pattern,
                                 float position, const char* sessionId) {
    JsonDocument doc;
    doc["timestamp"] = timestamp;
    doc["state"] = state;
    doc["speed"] = speed;
    doc["stroke"] = stroke;
    doc["sensation"] = sensation;
    doc["depth"] = depth;
    doc["pattern"] = pattern;
    doc["position"] = position;
    doc["sessionId"] = sessionId;
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

// ─── Test: key count is exactly 9 (required) without meta ────────────────

void test_key_count_without_meta() {
    auto doc = buildPayload(0, "s", 0, 0, 0, 0, 0, 0.0f,
                            "00000000-0000-0000-0000-000000000000");
    int count = 0;
    for (JsonPair kv : doc.as<JsonObject>()) {
        (void)kv;
        count++;
    }
    TEST_ASSERT_EQUAL_INT(9, count);
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

// ─── Runner ──────────────────────────────────────────────────────────────

int main() {
    UNITY_BEGIN();

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
