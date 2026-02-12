#include "wm.h"

#include "WiFi.h"
#include "esp_log.h"
#include "esp_wifi.h"

WiFiManager wm;

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
