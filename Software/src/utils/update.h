#ifndef SOFTWARE_UPDATE_H
#define SOFTWARE_UPDATE_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "ArduinoJson.h"
#include "constants/LogTags.h"

#ifndef SW_VERSION
#define SW_VERSION "0.0.0"
#endif

static auto isUpdateAvailable = []() {
    // check if we're online
    if (WiFiClass::status() != WL_CONNECTED) {
        ESP_LOGD(UPDATE_TAG, "Not connected to WiFi");
        return false;
    }

    String serverNameBubble =
        "http://d2g4f7zewm360.cloudfront.net/check-for-ossm-update";  // live
                                                                      // url
#ifdef VERSIONDEV
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
    WiFiClient client;
    http.begin(client, serverNameBubble);
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
    client.stop();
    return response_needUpdate;
};

auto updateOSSM = []() {
    // check if we're online

    WiFiClient client;
    String url = "http://d2sy3zdr3r1gt5.cloudfront.net/firmware.bin";

#ifdef VERSIONDEV
    url = "http://d2sy3zdr3r1gt5.cloudfront.net/firmware-dev.bin";
#endif
#ifdef VERSIONSTAGING
    url = "http://d2sy3zdr3r1gt5.cloudfront.net/firmware-dev.bin";
#endif

    t_httpUpdate_return ret = httpUpdate.update(
        client, url);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            ESP_LOGD("UTILS", "HTTP_UPDATE_FAILED Error (%d): %s\n",
                     httpUpdate.getLastError(),
                     httpUpdate.getLastErrorString().c_str());
            break;

        case HTTP_UPDATE_NO_UPDATES:
            ESP_LOGD("UTILS", "HTTP_UPDATE_NO_UPDATES");
            break;

        case HTTP_UPDATE_OK:
            ESP_LOGD("UTILS", "HTTP_UPDATE_OK");
            break;
    }

    client.stop();
};

#endif  // SOFTWARE_UPDATE_H
