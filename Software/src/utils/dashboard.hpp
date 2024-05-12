#ifndef SOFTWARE_DASHBOARD_HPP
#define SOFTWARE_DASHBOARD_HPP

#ifndef ENVIRONMENT_TOKEN
#define ENVIRONMENT_TOKEN                                                      \
    "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."                                    \
    "eyJlbnZpcm9ubWVudCI6ImRldmVsb3BtZW50IiwidmVyc2lvbiI6InYyLjEuMCJ9.ifQXF2_" \
    "sAmeCP_eixEr-br-c222bFJt9OvZN6x9MOiM"
#endif

#include <Arduino.h>

#include "services/preferences.h"
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
AuthResponse getAuthToken() {
    String deviceId = getId();
    String environmentToken = ENVIRONMENT_TOKEN;

    StaticJsonDocument<256> doc;
    doc["deviceId"] = deviceId;
    doc["environmentToken"] = environmentToken;
    String requestBody;
    serializeJson(doc, requestBody);

    ESP_LOGD("UTILS", "Request body: %s", requestBody.c_str());

    // TODO: Send the request to the dashboard
    String sessionId = generateId();

    // The response is a signed JWT which contains the ClientId and your
    // Username.
    return {.clientId = "ABC123",
            .username = "test",
            .password =
                "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
                "eyJjbGllbnRpZCI6IkFCQzEyMyIsInVzZXJuYW1lIjoidGVzdCJ9."
                "32bDZcbWL6uZYoV_ZmCn7Rkb7khs4noCmLiiNB8DBZQ",
            .sessionId = sessionId};
}

#endif  // SOFTWARE_DASHBOARD_HPP
