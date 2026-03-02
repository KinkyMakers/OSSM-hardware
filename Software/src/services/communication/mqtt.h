#ifndef LOCKBOX_MQTT_H
#define LOCKBOX_MQTT_H

#include <WiFi.h>
#include <mqtt_client.h>

extern bool mqttConnected;
extern esp_mqtt_client_handle_t mqttClient;
extern String sessionId;

void initMQTT();

#endif  // LOCKBOX_MQTT_H
