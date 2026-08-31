#pragma once

#include <Arduino.h>
#include <cmath>

namespace telemetry {

struct Snapshot {
    unsigned long timestamp;
    const char *state;
    float speed;
    float stroke;
    float sensation;
    float depth;
    float buffer;
    int pattern;
    float positionMm;
    const char *sessionId;
    const char *firmwareProvenanceId;
};

// Shared by dashboard MQTT and full BLE characteristic reads. Preserve the
// established field order, integer percentages and two-decimal position.
inline String serialize(const Snapshot &snapshot) {
    const float positionMm =
        std::isnan(snapshot.positionMm) ? 0.0f : snapshot.positionMm;
    return "{\"timestamp\":" + String(snapshot.timestamp) +
           ",\"state\":\"" + snapshot.state +
           "\",\"speed\":" + String((int)snapshot.speed) +
           ",\"stroke\":" + String((int)snapshot.stroke) +
           ",\"sensation\":" + String((int)snapshot.sensation) +
           ",\"depth\":" + String((int)snapshot.depth) +
           ",\"buffer\":" + String((int)snapshot.buffer) +
           ",\"pattern\":" + String(snapshot.pattern) +
           ",\"position\":" + String(positionMm, 2) +
           ",\"sessionId\":\"" + snapshot.sessionId +
           "\",\"firmwareProvenanceId\":\"" + snapshot.firmwareProvenanceId + "\"}";
}

}  // namespace telemetry
