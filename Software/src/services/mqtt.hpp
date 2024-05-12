#ifndef SOFTWARE_MQTT_H
#define SOFTWARE_MQTT_H

#include "WiFi.h"
#include "PubSubClient.h"

static WiFiClient wiFiClient;
static PubSubClient client(wiFiClient);


static String clientState() {
    int errorCode = client.state();
    switch (errorCode) {
        case -4:
            return "MQTT_CONNECTION_TIMEOUT - the server didn't respond within the keepalive time";
        case -3:
            return "MQTT_CONNECTION_LOST - the network connection was broken";
        case -2:
            return "MQTT_CONNECT_FAILED - the network connection failed";
        case -1:
            return "MQTT_DISCONNECTED - the client is disconnected cleanly";
        case 0:
            return "MQTT_CONNECTED - the client is connected";
        case 1:
            return "MQTT_CONNECT_BAD_PROTOCOL - the server doesn't support the requested version of MQTT";
        case 2:
            return "MQTT_CONNECT_BAD_CLIENT_ID - the server rejected the client identifier";
        case 3:
            return "MQTT_CONNECT_UNAVAILABLE - the server was unable to accept the connection";
        case 4:
            return "MQTT_CONNECT_BAD_CREDENTIALS - the username/password were rejected";
        case 5:
            return "MQTT_CONNECT_UNAUTHORIZED - the client was not authorized to connect";
        default:
            return "Unknown MQTT connection error";
    }
}


#endif  // SOFTWARE_MQTT_H
