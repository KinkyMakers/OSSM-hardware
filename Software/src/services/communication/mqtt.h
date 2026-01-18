#ifndef LOCKBOX_MQTT_H
#define LOCKBOX_MQTT_H

#include <WiFi.h>
#include <mqtt_client.h>

// Declare externals (no static here)
extern bool mqttConnected;
extern esp_mqtt_client_handle_t mqttClient;
extern const char* mqtt_server;
extern const int mqtt_port;
extern String sessionId;

void initMQTT();

#endif  // LOCKBOX_MQTT_H
