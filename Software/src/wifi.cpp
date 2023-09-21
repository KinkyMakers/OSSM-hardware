#include <ESPConnect.h>
#include <Crypto.h>
#include <SHA256.h>
#include <mdns.h>

#include "config.h"
#include "wifi.hpp"
#include "webserver.hpp"

// TODO - Get rid of these globals eventually
char wifiName[32];
char wifiPassword[16];

#if LVGL_AVAILABLE == 1
#include "lvgl_gui.hpp"
#include "screen/wifi_ap.hpp"
#include "screen/wifi_sta.hpp"
#include "screen/wifi_failure.hpp"
WifiStaScreen* wifiStaScreen = NULL;
WifiApScreen* wifiApScreen = NULL;
WifiFailureScreen* wifiFailureScreen = NULL;
void wifi_gui_poll(void* pvParameter) {
  LVGLGui* gui = LVGLGui::getInstance();
  wifiStaScreen = new WifiStaScreen();
  wifiApScreen = new WifiApScreen();
  wifiFailureScreen = new WifiFailureScreen();

  while (true) {
    // If WiFi is Connected, move on to Status
    if (WiFi.status() == WL_CONNECTED) {
      log_i("Wifi Polling Task exiting");
      vTaskDelete(NULL);
    } else if (WiFi.getMode() == WIFI_AP_STA && !wifiApScreen->getIsActive()) { 
      gui->activate(wifiApScreen);
    } else if (WiFi.getMode() == WIFI_STA  && !wifiStaScreen->getIsActive()) {
      gui->activate(wifiStaScreen);
    }

    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}
#endif

void wifi_mdns_poll(void* pvParameter) {
  esp_ip4_addr_t addr;
  addr.addr = 0;

  while (true) {
    if (WiFi.status() == WL_CONNECTED) {
      log_i("Wifi Polling Task exiting");
      vTaskDelete(NULL);
    }

    esp_err_t err = mdns_query_a("m5remote", 250,  &addr);
    vTaskDelay(33 / portTICK_PERIOD_MS);
  }
}

#define HASH_SIZE 32
SHA256 sha256;
void wifi_setup () {
  LVGLGui* gui = LVGLGui::getInstance();

  const char* mac = WiFi.macAddress().c_str();
  uint8_t value[HASH_SIZE];

  // TODO - Should change this as I'm pretty sure MAC address is broadcast over WiFi to everyone?
  // Instead just initialize a random value and store in preferences on first boot
  Hash *hash = &sha256;
  hash->reset();
  hash->update(mac, strlen(mac));
  hash->finalize(value, sizeof(value));

  sprintf(wifiName, "OSSM-SoftAP %02X", value[4]);
  sprintf(wifiPassword, "softap-%02x%02x", value[5], value[6]);

  log_i("Wifi: %s %s", wifiName, wifiPassword);
  log_i("MAC Address: %s", WiFi.macAddress().c_str());
  ESPConnect.autoConnect(wifiName, wifiPassword);

#if LVGL_AVAILABLE == 1
  // Start the WiFi checking task
  TaskHandle_t wifiGuiPollTask;
  Config::functionality.read([&](FunctionalityState& state) {
    if (state.lvglEnabled) {
      xTaskCreatePinnedToCore(&wifi_gui_poll, "wifi_gui_poll", 4096, NULL, 5, &wifiGuiPollTask, 1);
    }
  });
#endif

  TaskHandle_t wifiMdnsPollTask;
  xTaskCreatePinnedToCore(&wifi_mdns_poll, "wifi_mdns_poll", 4096, NULL, 5, &wifiMdnsPollTask, 1);

  // TODO - Reduce connectino time to less than 30 seconds?
  // TODO - Can we attempt to connect to configured AP, AND host an AP for connecting m5remote? WIFI_AP_STA mode
  // 30 second attempt to connect to configured AP
  // 180 second attempt to serve AP page for configuring WiFi
  // This will halt until WiFi.status() == WL_CONNECTED, or a timeout
  // Returns true if connected, false if timed out
  bool success = ESPConnect.begin(&server, WIFI_CONNECTION_TIMEOUT);

  if (!success) {
    vTaskDelete(wifiGuiPollTask);
    vTaskDelete(wifiMdnsPollTask);
    gui->activate(wifiFailureScreen);
    log_e("Failed to connect to WiFi. Waiting for reboot...");
    while (true) { vTaskDelay(50 / portTICK_PERIOD_MS); }
  }
}