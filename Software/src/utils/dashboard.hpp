#ifndef SOFTWARE_DASHBOARD_HPP
#define SOFTWARE_DASHBOARD_HPP

#ifndef ENVIRONMENT_TOKEN
#define ENVIRONMENT_TOKEN                                                      \
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."                                    \
    "eyJlbnZpcm9ubWVudCI6ImRldmVsb3BtZW50IiwidmVyc2lvbiI6InYyLjEuMCJ9.ifQXF2_" \
    "sAmeCP_eixEr-br-c222bFJt9OvZN6x9MOiM"
#endif

#include <Arduino.h>
#include <HTTPClient.h>

#include "constants/dashboard.h"
#include "utils/uniqueId.h"

struct AuthResponse {
    String clientId = "";
    String username = "";
    String password = "";
    String sessionId = "";
};

// This will send your environment token and your device id to the dashboard.
// If your device has been paired with the dashboard, it will return a session
// token The session token will be used for MQTT communication.
static AuthResponse getAuthToken() {
    String deviceId = getId();
    String environmentToken = ENVIRONMENT_TOKEN;

    StaticJsonDocument<256> doc;
    doc["deviceId"] = deviceId;

    String requestBody;
    serializeJson(doc, requestBody);
    // post to the dashboard
    // 1. Create HTTP Client

    HTTPClient http;
    http.begin(RAD_AUTH);
    http.addHeader("Content-Type", "application/json");
    // add body
    int httpResponseCode = http.POST(requestBody);
    // read response
    String payload = "{}";
    payload = http.getString();
    http.end();

    // JSON Parse the response
    StaticJsonDocument<1024> response;
    deserializeJson(response, payload);

    ESP_LOGD("UTILS", "HTTP Response code: %d", httpResponseCode);
    ESP_LOGD("UTILS", "Response: %s", payload.c_str());

    // The response is a signed JWT which contains the ClientId and your
    // Username.
    return {.clientId = response["clientId"],
            .username = response["username"],
            .password = response["password"],
            .sessionId = response["sessionId"]};
}

#endif  // SOFTWARE_DASHBOARD_HPP
