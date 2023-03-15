#include "esp_log.h"
#include "config.hpp"

// General hardware libs
#include "SPIFFS.h"
#include "Wire.h"
#include <Preferences.h>
#include <Crypto.h>
#include <SHA256.h>

// Specific hardware libs
#include "ESPAsyncWebServer.h"
#include <ESPDash.h>
#include <FastLED.h>

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

#include "reset_reason.hpp"

#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

#if MOTOR_USED == MOTOR_LINMOT
  #include "motor/linmot.hpp"
  LinmotMotor* motor;
#elif MOTOR_USED == MOTOR_MOCK
  #include "motor/virtualMotor.h"
  VirtualMotor* motor;
#endif

StrokeEngine* engine;

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
}

BootScreen* bootScreen = NULL;
void boot_start () {
  bootScreen = new BootScreen();
  gui->activate(bootScreen);

  // Allow the boot screen to show for a bit
  for (uint8_t i = 0; i < (BOOT_SCREEN_WAIT / 50); i++) { vTaskDelay(50 / portTICK_PERIOD_MS); }
}


void storage_setup() {
  ESP_LOGI("main", "Mounting SPIFFS");
  if(!SPIFFS.begin(true)){
    ESP_LOGE("main", "An Error has occurred while mounting SPIFFS");
    return;
  }
}

void espnow_setup() {
  WiFi.mode(WIFI_STA);
  ESP_LOGI("main", "MAC Address: %s", WiFi.macAddress());

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    ESP_LOGE("main", "Error initializing ESP-NOW");
    return;
  }
}

/*
void blynk_setup() {
  ESP_LOGI("main", "Starting Blynk!");
  blynk = new BlynkController();
}
*/

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

FunctionalityStateService functionalityStateService;
void setup() {
  boot_setup();
  boot_start();

  gui = new LVGLGui();
  gui->start();

  functionalityStateService.read([&](FunctionalityState& state) {
    if (state.)
  });

  wifi_setup();
  //blynk_setup();
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
