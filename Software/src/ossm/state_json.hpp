#ifndef OSSM_STATE_JSON_HPP
#define OSSM_STATE_JSON_HPP

#include <Arduino.h>
#include <ArduinoJson.h>

// Builder for the state payload shared by MQTT telemetry and the BLE state
// characteristic. Kept free of hardware includes so it is unit-tested
// natively (test/test_state_json). The base keys are a contract with the
// dashboard telemetry schema; the optional keys are BLE-only hints for the
// RADR and are omitted when unset.
//
// Serialized with ArduinoJson rather than String concatenation: long
// Arduino String "+" chains were seen emitting a doubled comma on hardware
// (2026-09-09), which breaks every consumer of this payload.
struct StateJsonInput {
    unsigned long timestamp = 0;
    String state;
    int speed = 0;
    int stroke = 0;
    int sensation = 0;
    int depth = 0;
    int buffer = 0;
    int pattern = 0;
    float position = 0.0f;
    String sessionId;
    String firmwareProvenanceId;  // may be empty; always emitted (dashboard contract)
    // optional
    String error;
    String pairingCode;
    bool isPaired = false;
    String targetVersion;
};

inline String buildStateJson(const StateJsonInput& in) {
    JsonDocument doc;
    doc["timestamp"] = in.timestamp;
    doc["state"] = in.state;
    doc["speed"] = in.speed;
    doc["stroke"] = in.stroke;
    doc["sensation"] = in.sensation;
    doc["depth"] = in.depth;
    doc["buffer"] = in.buffer;
    doc["pattern"] = in.pattern;
    doc["position"] = serialized(String(in.position, 2));
    doc["sessionId"] = in.sessionId;
    doc["firmwareProvenanceId"] = in.firmwareProvenanceId;
    if (in.error.length() > 0) doc["error"] = in.error;
    if (in.pairingCode.length() > 0) doc["pairingCode"] = in.pairingCode;
    if (in.isPaired) doc["isPaired"] = true;
    if (in.targetVersion.length() > 0) doc["targetVersion"] = in.targetVersion;
    // Fixed buffer: serializing straight into an Arduino String needs
    // Print::write, which the native test harness's String lacks.
    char out[384];
    serializeJson(doc, out, sizeof(out));
    return String(out);
}

#endif  // OSSM_STATE_JSON_HPP
