#ifndef SOFTWARE_DASHBOARD_H
#define SOFTWARE_DASHBOARD_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>

#include "ArduinoJson.h"

static void uploadJson(const String& jsonContent) {
    if (WiFiClass::status() != WL_CONNECTED) {
        return;
    }

    HTTPClient http;
    WiFiClient client;

    http.begin(client,
               "http://0d6ef628-c152-44f3-8401-48a1c31b68ed.mock.pstmn.io");
    http.addHeader("Content-Type", "application/json");

    int httpResponseCode = http.POST(jsonContent);
    if (httpResponseCode > 0) {
        String response = http.getString();

        ESP_LOGD("UTILS", "HTTP Response code: %d", httpResponseCode);
        ESP_LOGD("UTILS", "Response: %s", response.c_str());
    } else {
        ESP_LOGD("UTILS", "Error on sending POST: %d", httpResponseCode);
    }
    http.end();
    client.stop();
}

struct Points {
    int x;
    int y;
    int t;
};

struct DashboardData {
    String id;
    String sessionId;
    Points data[100];
};

// task for posting to the dashboard
[[noreturn]] static void postToDashboardTask(void* parameter) {
    DashboardData data;
    data.id = "75c11050-0e3f-495a-a09b-2f9a947adb8a";
    data.sessionId = "75c11050-0e3f-495a-a09b-2f9a947adb8a";
    int n = 0;
    const int nPoints = 100;
    while (true) {
        vTaskDelay(100 / portTICK_PERIOD_MS);

        //        if (n < nPoints) {
        //            data.data[n].x = n * 100;
        //            data.data[n].y = n * n * 100;
        //            data.data[n].t = 1000 * 60 * 60 * 24;
        //            n++;
        //            continue;
        //        }
        //        String jsonContent;
        //        jsonContent = String(R"({"id":")") + data.id +
        //                      String(R"(","sessionId":")") + data.sessionId +
        //                      String(R"(","data":[)");
        //        for (int i = 0; i < nPoints; i++) {
        //            jsonContent += String(R"({"x":")") + data.data[i].x +
        //                           String(R"(","y":")") + data.data[i].y +
        //                           String(R"(","t":")") + data.data[i].t +
        //                           String(R"("})");
        //            if (i < nPoints - 1) {
        //                jsonContent += ",";
        //            }
        //        }
        //        jsonContent += "]}";
        //        uploadJson(jsonContent);
        //        n = 0;
    }
}

// http post to the dashboard
static void postToDashboard() {
    int stackSize = 1 * configMINIMAL_STACK_SIZE;
    xTaskCreate(postToDashboardTask, "postToDashboardTask", stackSize, nullptr, 0, nullptr);
}

#endif  // SOFTWARE_DASHBOARD_H
