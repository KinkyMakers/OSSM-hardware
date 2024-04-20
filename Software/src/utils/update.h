#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H
#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "Arduino.h"
#include "OSSM_Config.h"
#include "esp_log.h"

bool checkForUpdate() {
    String serverNameBubble =
        "http://d2g4f7zewm360.cloudfront.net/check-for-ossm-update";  // live
                                                                      // url
#ifdef VERSIONTEST
    serverNameBubble =
        "http://d2oq8yqnezqh3r.cloudfront.net/check-for-ossm-update";  // version-test
#endif
    ESP_LOGD("UTILS", "about to hit http for update");
    HTTPClient http;
    http.begin(serverNameBubble);
    http.addHeader("Content-Type", "application/json");
    StaticJsonDocument<200> doc;
    // Add values in the document
    doc["ossmSwVersion"] = SW_VERSION;

    String requestBody;
    serializeJson(doc, requestBody);
    ESP_LOGD("UTILS", "about to POST");
    int httpResponseCode = http.POST(requestBody);
    ESP_LOGD("UTILS", "POSTed");
    String payload = "{}";
    payload = http.getString();
    ESP_LOGD("UTILS", "HTTP Response code: %d", httpResponseCode);
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
