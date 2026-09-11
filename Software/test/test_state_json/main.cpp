// State JSON contract for the BLE state characteristic and MQTT telemetry.
// The base keys must stay identical to test_mqtt_payload (dashboard schema);
// the optional keys (error, pairingCode, isPaired) are BLE hints for the RADR
// and must be absent unless set.
#include <ArduinoFake.h>
#include <ArduinoJson.h>
#include <unity.h>

#include "ossm/state_json.hpp"

static StateJsonInput baseInput() {
    StateJsonInput in;
    in.timestamp = 12345;
    in.state = "menu.idle";
    in.speed = 10;
    in.stroke = 20;
    in.sensation = 30;
    in.depth = 40;
    in.buffer = 50;
    in.pattern = 2;
    in.position = 12.5f;
    in.sessionId = "0f1e2d3c-0000-4000-8000-000000000000";
    return in;
}

static JsonDocument parse(const String& json) {
    JsonDocument doc;
    TEST_ASSERT_TRUE(deserializeJson(doc, json.c_str()) == DeserializationError::Ok);
    return doc;
}

void test_base_payload_has_exact_keys() {
    JsonDocument doc = parse(buildStateJson(baseInput()));
    const char* expected[] = {"timestamp", "state",   "speed",    "stroke",
                              "sensation", "depth",   "buffer",   "pattern",
                              "position",  "sessionId", "firmwareProvenanceId"};
    for (const char* key : expected) {
        TEST_ASSERT_TRUE_MESSAGE(doc[key].is<JsonVariant>(), key);
    }
    TEST_ASSERT_EQUAL(11, doc.as<JsonObject>().size());
    TEST_ASSERT_FALSE(doc["error"].is<JsonVariant>());
    TEST_ASSERT_FALSE(doc["pairingCode"].is<JsonVariant>());
    TEST_ASSERT_FALSE(doc["isPaired"].is<JsonVariant>());
}

void test_base_values_round_trip() {
    JsonDocument doc = parse(buildStateJson(baseInput()));
    TEST_ASSERT_EQUAL(12345, doc["timestamp"].as<long>());
    TEST_ASSERT_EQUAL_STRING("menu.idle", doc["state"].as<const char*>());
    TEST_ASSERT_EQUAL(10, doc["speed"].as<int>());
    TEST_ASSERT_EQUAL(2, doc["pattern"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 12.5f, doc["position"].as<float>());
}

void test_error_is_reported_when_set() {
    StateJsonInput in = baseInput();
    in.state = "update.failed";
    in.error = "low-memory";
    JsonDocument doc = parse(buildStateJson(in));
    TEST_ASSERT_EQUAL_STRING("update.failed", doc["state"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("low-memory", doc["error"].as<const char*>());
    TEST_ASSERT_EQUAL(12, doc.as<JsonObject>().size());
}

void test_pairing_fields_reported_when_set() {
    StateJsonInput in = baseInput();
    in.state = "pairing.idle";
    in.pairingCode = "ABC123";
    JsonDocument doc = parse(buildStateJson(in));
    TEST_ASSERT_EQUAL_STRING("ABC123", doc["pairingCode"].as<const char*>());
    TEST_ASSERT_FALSE(doc["isPaired"].is<JsonVariant>());

    in.isPaired = true;
    doc = parse(buildStateJson(in));
    TEST_ASSERT_TRUE(doc["isPaired"].as<bool>());
}

void test_target_version_reported_when_set() {
    StateJsonInput in = baseInput();
    in.state = "update.available";
    in.targetVersion = "1.0.58";
    JsonDocument doc = parse(buildStateJson(in));
    TEST_ASSERT_EQUAL_STRING("1.0.58", doc["targetVersion"].as<const char*>());
    in.targetVersion = "";
    doc = parse(buildStateJson(in));
    TEST_ASSERT_FALSE(doc["targetVersion"].is<JsonVariant>());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_base_payload_has_exact_keys);
    RUN_TEST(test_base_values_round_trip);
    RUN_TEST(test_error_is_reported_when_set);
    RUN_TEST(test_pairing_fields_reported_when_set);
    RUN_TEST(test_target_version_reported_when_set);
    return UNITY_END();
}
