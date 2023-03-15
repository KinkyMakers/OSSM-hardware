#include "wifi.hpp"
#include "lvgl_gui.hpp"

#include <ESPConnect.h>

// TODO - Get rid of these globals eventually
char wifiName[32];
char wifiPassword[10];

AsyncWebServer server(80);
AsyncWebSocket ws("/");
AsyncEventSource events("/es");

WifiStaScreen* wifiStaScreen = NULL;
WifiApScreen* wifiApScreen = NULL;
WifiFailureScreen* wifiFailureScreen = NULL;
StatusScreen* statusScreen = NULL;
void wifi_poll(void* pvParameter) {
  LVGLGui* gui = LVGLGui::getInstance();

  while (true) {
    // If WiFi is Connected, move on to Status
    if (WiFi.status() == WL_CONNECTED) {
      ESP_LOGI("main", "Wifi Polling Task exiting");
      vTaskDelete(NULL);
    } else if (WiFi.getMode() == WIFI_AP_STA && !wifiApScreen->getIsActive()) { 
      gui->activate(wifiApScreen);
    } else if (WiFi.getMode() == WIFI_STA  && !wifiStaScreen->getIsActive()) {
      gui->activate(wifiStaScreen);
    }

    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}

#define HASH_SIZE 32
SHA256 sha256;
void wifi_setup () {
  LVGLGui* gui = LVGLGui::getInstance();

  wifiStaScreen = new WifiStaScreen();
  wifiApScreen = new WifiApScreen();
  wifiFailureScreen = new WifiFailureScreen();

  const char* mac = WiFi.macAddress().c_str();
  uint8_t value[HASH_SIZE];

  Hash *hash = &sha256;
  hash->reset();
  hash->update(mac, strlen(mac));
  hash->finalize(value, sizeof(value));

  sprintf(wifiName, "CAN-SoftAP %02X", value[4]);
  sprintf(wifiPassword, "softap-%02x%02x", value[5], value[6]);

  ESP_LOGI("main", "Wifi: %s %s", wifiName, wifiPassword);
  ESP_LOGI("main", "MAC Address: %s", WiFi.macAddress());
  ESPConnect.autoConnect(wifiName, wifiPassword);

  // Start the WiFi checking task
  TaskHandle_t wifiPollTask;
  xTaskCreatePinnedToCore(&wifi_poll, "wifi_poll", 4096, NULL, 5, &wifiPollTask, 1);

  // 30 second attempt to connect to configured AP
  // 180 second attempt to serve AP page for configuring WiFi
  // This will halt until WiFi.status() == WL_CONNECTED, or a timeout
  // Returns true if connected, false if timed out
  bool success = ESPConnect.begin(&server);

  if (!success) {
    vTaskDelete(wifiPollTask);
    gui->activate(wifiFailureScreen);
    ESP_LOGE("main", "Failed to connect to WiFi. Waiting for reboot...");
    while (true) { vTaskDelay(50 / portTICK_PERIOD_MS); }
  }
}