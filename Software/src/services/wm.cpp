#include "wm.h"

#include <ArduinoJson.h>
#include <Preferences.h>
#include "WiFi.h"
#include "esp_log.h"
#include "esp_wifi.h"

WiFiManager wm;
Preferences wifiPrefs;

void initWM() {
    WiFi.useStaticBuffers(true);
    esp_wifi_set_ps(WIFI_PS_MAX_MODEM);

    wm.setSaveConfigCallback([]() {
        ESP_LOGI("WM", "WiFi credentials saved to NVS");
    });

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
    WiFi.begin();
#endif

    ESP_LOGI("WM", "WiFi initialization complete, status: %d", WiFi.status());
}

bool setWiFiCredentials(const String& ssid, const String& password) {
    // Open preferences in read/write mode
    if (!wifiPrefs.begin("wifi", false)) {
        ESP_LOGE("WM", "Failed to open preferences for writing");
        return false;
    }

    // Save credentials to NVS
    wifiPrefs.putString("ssid", ssid);
    wifiPrefs.putString("password", password);
    wifiPrefs.end();

    ESP_LOGI("WM", "WiFi credentials saved to NVS");
    return true;
}

bool connectWiFi() {
    // Read credentials from NVS
    if (!wifiPrefs.begin("wifi", true)) {
        ESP_LOGE("WM", "Failed to open preferences for reading");
        return false;
    }

    String ssid = wifiPrefs.getString("ssid", "");
    String password = wifiPrefs.getString("password", "");
    wifiPrefs.end();

    if (ssid.length() == 0) {
        ESP_LOGW("WM", "No SSID found in NVS");
        return false;
    }

    ESP_LOGI("WM", "Attempting to connect to WiFi: %s", ssid.c_str());

    // Disconnect if already connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect();
        delay(100);
    }

    // Begin connection
    WiFi.begin(ssid.c_str(), password.c_str());

    // Wait for connection (timeout after 10 seconds)
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        attempts++;
        ESP_LOGD("WM", "Connecting... attempt %d/20", attempts);
    }

    if (WiFi.status() == WL_CONNECTED) {
        ESP_LOGI("WM", "Connected to WiFi. IP: %s", WiFi.localIP().toString().c_str());
        return true;
    } else {
        ESP_LOGW("WM", "Failed to connect to WiFi. Status: %d", WiFi.status());
        return false;
    }
}

String getWiFiStatus() {
    JsonDocument doc;
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    doc["connected"] = connected;

    if (connected) {
        doc["ssid"] = WiFi.SSID();
        doc["ip"] = WiFi.localIP().toString();
        doc["rssi"] = WiFi.RSSI();
    } else {
        // Try to read saved SSID from NVS
        if (wifiPrefs.begin("wifi", true)) {
            String savedSSID = wifiPrefs.getString("ssid", "");
            if (savedSSID.length() > 0) {
                doc["ssid"] = savedSSID;
            }
            wifiPrefs.end();
        }
        doc["ip"] = "";
        doc["rssi"] = 0;
    }

    String output;
    serializeJson(doc, output);
    return output;
}
