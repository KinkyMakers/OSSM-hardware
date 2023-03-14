#include "esp_log.h"
#include "config.h"

// General hardware libs
#include "SPIFFS.h"
#include "Wire.h"
#include <Preferences.h>
#include <Crypto.h>
#include <SHA256.h>

// Specific hardware libs
#include <ESPConnect.h>
#include "ESPAsyncWebServer.h"
#include <ESPDash.h>
#include <FastLED.h>

#include "blynk.hpp"
#include "StrokeEngine.h"

#include "controller/canfuck.hpp"

#include "CO_main.h"
#include "lvgl_gui.hpp"
#include "wifi.hpp"

// Screens
#include "screen/boot.hpp"
#include "screen/status.hpp"
#include "screen/wifi_ap.hpp"
#include "screen/wifi_sta.hpp"
#include "screen/wifi_failure.hpp"

#include "data_logger.hpp"
#include "reset_reason.hpp"

#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

AsyncWebServer server(80);
AsyncWebSocket ws("/");
AsyncEventSource events("/es");

// TODO - Allow ESP32 Perferences to choose motor implementation

#if MOTOR_USED == MOTOR_LINMOT
  #include "motor/linmot.hpp"
  LinmotMotor* motor;
#elif MOTOR_USED == MOTOR_MOCK
  #include "motor/virtualMotor.h"
  VirtualMotor* motor;
#endif

StrokeEngine* engine;
CANFuckController* controller; // TODO - Abstract interface with controller away? Use similar system to Object Dictionary with CANOpen?
BlynkController* blynk = NULL;

void onRequest(AsyncWebServerRequest *request){
  //Handle Unknown Request
  request->send(404);
}

void onBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
  //Handle body
}

void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  //Handle upload
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      // TODO - Might support incoming data at some point, but not a concern right now
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void onConnect(AsyncEventSourceClient *client) {
    if(client->lastId()){
      Serial.printf("EventSource Client reconnected! Last message ID that it gat is: %u\n", client->lastId());
    } else {
      Serial.printf("EventSource Client connected!");
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!",NULL,millis(),1000);
}


// States
// Booting - Show Logo for a few seconds
// Connecting to AP
// Starting SoftAP

// BOOT (Serial, I2C, Core Dump / CPU Reset Reason)
// WIFI_CONNECT (Attempt WiFi connection for 1 min, then abort)
// WIFI_SOFT_AP_START (Idle in this state for a minute)
// COREDUMP_SEND (Optional if WiFi starts)
// WIFI_SERVER_START (Optional if WiFi starts)
// MOTOR_START
// ENGINE_START
// ACTIVE
// ERROR

LVGLGui* gui;
void loop() {
  if (blynk != NULL) {
    blynk->loop();
  }

  vTaskDelay(200 / portTICK_PERIOD_MS);
  // TODO - fix this stack overflow
  //ws.cleanupClients();
}

void boot_setup() {
  Serial.begin(UART_SPEED);
  print_reset_reason();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  
  pinMode(13, OUTPUT);
  digitalWrite(13, HIGH);
  FastLED.addLeds<NEOPIXEL, 21>(leds, NUM_LEDS);
  leds[0] = CHSV(0, 180, 64);
  FastLED.show(); 

#if ENABLE_LVGL == FUNCTIONALITY_ENABLED
  gui = new LVGLGui();
  gui->start();
#endif
}

BootScreen* bootScreen = NULL;
void boot_start () {
  bootScreen = new BootScreen();
  gui->activate(bootScreen);

  // Allow the boot screen to show for a bit
  for (uint8_t i = 0; i < (BOOT_SCREEN_WAIT / 50); i++) { vTaskDelay(50 / portTICK_PERIOD_MS); }
}

WifiStaScreen* wifiStaScreen = NULL;
WifiApScreen* wifiApScreen = NULL;
WifiFailureScreen* wifiFailureScreen = NULL;
StatusScreen* statusScreen = NULL;
void wifi_poll(void* pvParameter) {
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

void storage_setup() {
  ESP_LOGI("main", "Mounting SPIFFS");
  if(!SPIFFS.begin(true)){
    ESP_LOGE("main", "An Error has occurred while mounting SPIFFS");
    return;
  }
}

void blynk_setup() {
  ESP_LOGI("main", "Starting Blynk!");
  blynk = new BlynkController();
}

void server_setup() {
  ESP_LOGI("main", "Starting Web Server");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));

  server.serveStatic("/www2", SPIFFS, "/www/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *request) {
      Serial.printf("Not found: %s!\r\n", request->url().c_str());
      request->send(404);
   });

  server.begin();

  ws.onEvent(onEvent);
  events.onConnect((ArEventHandlerFunction)onConnect);
  server.addHandler(&ws);
  server.addHandler(&events);
  wlog.attachWebsocket(&ws);
  wlog.attachEventsource(&events);
  wlog.startTask();
}

void setup() {
  boot_setup();
  boot_start();
  wifi_setup();
  blynk_setup();
  storage_setup();
  server_setup();

  statusScreen = new StatusScreen();
  gui->activate(statusScreen);

  if (CONTROLLER_USED) {
    WEB_LOGI("main", "Initializing Hardware Controller");
    controller = new CANFuckController();
    if (controller->start() == false) {
      WEB_LOGE("main", "Unable to initialize Hardware Controller");
      esp_restart();
    }
  } else {
    WEB_LOGI("main", "No Hardware Controller, only Blynk Controls will be available!");
  }

  WEB_LOGI("main", "Configuring Motor");

#if MOTOR_USED == MOTOR_LINMOT
  motor = new LinmotMotor();

  MachineGeometry bounds = {
    .start = 110, // mm
    .end = -7, // mm
    .keepout = 10 // mm
  };
  motor->setMaxSpeed(5000); // 5 m/s
  motor->setMaxAcceleration(500); // 25 m/s^2
  motor->setMachineGeometry(bounds);

  motor->CO_setNodeId(NODE_ID_LINMOT);
  motor->CO_setStatusUInt16(OD_ENTRY_H2110_linMotStatusUInt16);
  motor->CO_setMonitorSInt16(OD_ENTRY_H2114_linMotStatusSInt16);
  motor->CO_setMonitorSInt32(OD_ENTRY_H2115_linMotStatusSInt32);
  motor->CO_setControl(OD_ENTRY_H2111_linMotControlWord);
  motor->CO_setCmdHeader(OD_ENTRY_H2112_linMotCMD_Header);
  motor->CO_setCmdParameters(OD_ENTRY_H2113_linMotCMD_Parameters);
#elif MOTOR_USED == MOTOR_MOCK
  motor = new VirtualMotor();

  motor->setMaxSpeed(5000); // 5 m/s
  motor->setMaxAcceleration(500); // 25 m/s^2
  motor->setMachineGeometry(0, 100, 10);
#endif

  engine = new StrokeEngine();
  WEB_LOGI("main", "Attaching Motor to Stroke Engine");
  engine->attachMotor(motor);

  if (controller != NULL) {
  WEB_LOGI("main", "Attaching Controller to Stroke Engine");
    controller->attachEngine(engine);
  }

  WEB_LOGI("main", "Attaching Blynk to Motor and Engine");
  blynk->attach(engine, motor);

  WEB_LOGI("main", "Configuring Stroke Engine");
  engine->setParameter(StrokeParameter::RATE, 50);
  engine->setParameter(StrokeParameter::DEPTH, 100);
  engine->setParameter(StrokeParameter::STROKE, 50);
  engine->setParameter(StrokeParameter::SENSATION, 0);
  
  // TODO - Move controller tasks into init/attach
  if (controller != NULL) {
    controller->registerTasks();
  }

  WEB_LOGI("main", "Homing Motor");
  motor->enable();
  motor->home();
}
