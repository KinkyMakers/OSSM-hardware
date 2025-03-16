#ifndef OSSM_SOFTWARE_ESPNOW_H
#define OSSM_SOFTWARE_ESPNOW_H

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>

// Message format: ST:strokeEngine,SP:19,SK:0,SR:58,SN:18,DP:25,PT:2
struct StrokeMessage {
    String strokeType;  // ST - e.g., "strokeEngine"
    int speed;          // SP - Speed value
    int stroke;         // SK - Stroke value
    int strokeRate;     // SR - Stroke rate
    int strokeNumber;   // SN - Stroke number
    int depth;          // DP - Depth value
    int pattern;        // PT - Pattern value
};

// Callback function for when data is received
void onDataReceived(const uint8_t *mac, const uint8_t *data, int len);

// Initialize ESP-NOW
void initEspNow();

// Parse received message
StrokeMessage parseStrokeMessage(const String &message);

enum {
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

#endif  // OSSM_SOFTWARE_ESPNOW_H
