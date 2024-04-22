#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

#include <Arduino.h>
#include <HTTPClient.h>

#include "ArduinoJson.h"
#include "constants/LogTags.h"

#ifndef SW_VERSION
#define SW_VERSION "0.0.0"
#endif

bool isUpdateAvailable() {
    // check if we're online
    if (WiFiClass::status() != WL_CONNECTED) {
        ESP_LOGD(UPDATE_TAG, "Not connected to WiFi");
        return false;
    }

    String serverNameBubble =
        "http://d2g4f7zewm360.cloudfront.net/check-for-ossm-update";  // live
                                                                      // url
#ifdef VERSIONTEST
    serverNameBubble =
        "http://d2oq8yqnezqh3r.cloudfront.net/check-for-ossm-update";  // version-test
#endif

#ifdef VERSIONSTAGING
    serverNameBubble =
        "http://d2oq8yqnezqh3r.cloudfront.net/check-for-ossm-update";  // version-test
#endif

    ESP_LOGD(UPDATE_TAG, "Checking for updates at %s",
             serverNameBubble.c_str());

    // Making the POST request to the bubble server
    HTTPClient http;
    http.begin(serverNameBubble);
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    // Add values in the document
    doc["ossmSwVersion"] = SW_VERSION;
    String requestBody;
    serializeJson(doc, requestBody);
    int httpResponseCode = http.POST(requestBody);

    // Reading payload
    String payload = "{}";
    payload = http.getString();
    ESP_LOGD(UPDATE_TAG, "HTTP Response code: %d", httpResponseCode);
    StaticJsonDocument<200> bubbleResponse;
    deserializeJson(bubbleResponse, payload);
    bool response_needUpdate = bubbleResponse["response"]["needUpdate"];

    ESP_LOGD("UTILS", "Payload: %s", payload.c_str());

    if (httpResponseCode <= 0) {
        ESP_LOGD("UTILS", "Failed to reach update server");
    }
    http.end();
    return response_needUpdate;
}

#endif  // SOFTWARE_UPDATE_H
