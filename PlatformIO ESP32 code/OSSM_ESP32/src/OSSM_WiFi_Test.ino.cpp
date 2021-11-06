# 1 "/var/folders/j3/y9zm4wds63s395qmxncxgbtw0000gn/T/tmpxlyx5zvq"
#include <Arduino.h>
# 1 "/Users/jamescraig/Documents/GitHub/OSSM-hardware/PlatformIO ESP32 code/OSSM_ESP32/src/OSSM_WiFi_Test.ino"
#include <ArduinoJson.h>
#include <ESP_FlexyStepper.h>
#include <HTTPClient.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <Arduino.h>



#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "images.h"
#include <RotaryEncoder.h>


#define ENCODER_SWITCH 35
#define ENCODER_A 18
#define ENCODER_B 5
#define REMOTE_SDA 21
#define REMOTE_CLK 19
#define REMOTE_ADDRESS 0x3c



RotaryEncoder encoder(ENCODER_A, ENCODER_B, RotaryEncoder::LatchMode::TWO03);


SSD1306Wire display(REMOTE_ADDRESS, REMOTE_SDA, REMOTE_CLK);
OLEDDisplayUi ui(&display);

#include "ossm_ui.h"


WiFiManager wm;


ESP_FlexyStepper stepper;


volatile float strokePercentage = 0;
volatile float speedPercentage = 0;
volatile float deceleration = 0;




TaskHandle_t wifiTask = nullptr;
TaskHandle_t getInputTask = nullptr;
TaskHandle_t motionTask = nullptr;
TaskHandle_t estopTask = nullptr;
TaskHandle_t oledTask = nullptr;


#define MOTOR_STEP_PIN 27
#define MOTOR_DIRECTION_PIN 25
#define MOTOR_ENABLE_PIN 22

#define STROKE_POT_PIN A6
#define SPEED_POT_PIN 33

#define WIFI_RESET_PIN 0

#define WIFI_CONTROL_TOGGLE_PIN 26


#define WIFI_CONTROL_DEFAULT INPUT_PULLUP



#define STOP_PIN 19


#define LIMIT_SWITCH_PIN 21

#define DEBUG 


const float maxSpeedMmPerSecond = 1000;
const float motorStepPerRevolution = 1600;
const float pulleyToothCount = 20;
const float maxStrokeLengthMm = 150;
const float minimumCommandPercentage = 1.0f;

const float beltPitchMm = 2;



const float accelerationScaling = 80.0f;

const char *ossmId = "OSSM1";






void getUserInputTask(void *pvParameters);
void motionCommandTask(void *pvParameters);
float getAnalogAverage(int pinNumber, int samples);
bool setInternetControl(bool wifiControlEnable);
bool getInternetSettings();

bool stopSwitchTriggered = 0;
void ICACHE_RAM_ATTR stopSwitchHandler();
void setup();
void loop();
void oledUpdateTask(void *pvParameters);
void estopResetTask(void *pvParameters);
void wifiConnectionTask(void *pvParameters);
#line 111 "/Users/jamescraig/Documents/GitHub/OSSM-hardware/PlatformIO ESP32 code/OSSM_ESP32/src/OSSM_WiFi_Test.ino"
void ICACHE_RAM_ATTR stopSwitchHandler()
{
  stopSwitchTriggered = 1;
  vTaskSuspend(motionTask);
  vTaskSuspend(getInputTask);
  stepper.emergencyStop();
}

void setup()
{
  Serial.begin(115200);
#ifdef DEBUG
  Serial.println("\n Starting");
  delay(200);
#endif

  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  float stepsPerMm =
      motorStepPerRevolution /
      (pulleyToothCount * beltPitchMm);
  stepper.setStepsPerMillimeter(stepsPerMm);


  stepper.setSpeedInStepsPerSecond(100);
  stepper.setAccelerationInMillimetersPerSecondPerSecond(100);
  stepper.setDecelerationInStepsPerSecondPerSecond(100);

  WiFi.mode(WIFI_STA);

  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(WIFI_CONTROL_TOGGLE_PIN, WIFI_CONTROL_DEFAULT);


  pinMode(STOP_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(STOP_PIN), stopSwitchHandler, RISING);

  pinMode(STROKE_POT_PIN, INPUT);
  adcAttachPin(STROKE_POT_PIN);

  pinMode(SPEED_POT_PIN, INPUT);
  adcAttachPin(SPEED_POT_PIN);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);


  if (digitalRead(WIFI_RESET_PIN) == LOW)
  {

    wm.resetSettings();
#ifdef DEBUG
    Serial.println("settings reset");
#endif
  }





  ui.setTargetFPS(5);


  ui.setActiveSymbol(activeSymbol);
  ui.setInactiveSymbol(inactiveSymbol);



  ui.setIndicatorPosition(LEFT);


  ui.setIndicatorDirection(LEFT_RIGHT);



  ui.setFrameAnimation(SLIDE_LEFT);


  ui.setFrames(frames, frameCount);


  ui.setOverlays(overlays, overlaysCount);


  ui.init();
  ui.disableAutoTransition();

  display.flipScreenVertically();






  stepper.startAsService();







  xTaskCreatePinnedToCore(
      wifiConnectionTask,
      "wifiConnectionTask",
      10000,
      NULL,
      1,
      &wifiTask,
      0);
  delay(5000);
  xTaskCreatePinnedToCore(
      getUserInputTask,
      "getUserInputTask",
      10000,
      NULL,
      1,
      &getInputTask,
      0);
  delay(500);
  xTaskCreatePinnedToCore(
      motionCommandTask,
      "motionCommandTask",
      10000,
      NULL,
      1,
      &motionTask,
      0);

  delay(500);
  xTaskCreatePinnedToCore(
      estopResetTask,
      "estopResetTask",
      10000,
      NULL,
      1,
      &estopTask,
      0);

  delay(500);

  xTaskCreatePinnedToCore(
      oledUpdateTask,
      "oledUpdateTask",
      10000,
      NULL,
      2,
      &oledTask,
      0);

  delay(500);
}

void loop()
{

  ui.update();


}

void oledUpdateTask(void *pvParameters)
{

  for (;;)
  {

    vTaskDelay(10);
  }

}

void estopResetTask(void *pvParameters)
{
  for (;;)
  {
    if (stopSwitchTriggered == 1)
    {
      while ((getAnalogAverage(SPEED_POT_PIN, 50) + getAnalogAverage(STROKE_POT_PIN, 50)) > 2)
      {
        vTaskDelay(1);
      }
      stopSwitchTriggered = 0;
      vTaskResume(motionTask);
      vTaskResume(getInputTask);
    }
    vTaskDelay(100);
  }
}

void wifiConnectionTask(void *pvParameters)
{
  wm.setConfigPortalTimeout(1);
  wm.setConfigPortalBlocking(false);


  if (!wm.autoConnect("OSSM-setup"))
  {

#ifdef DEBUG
    Serial.println("failed to connect and hit timeout");
#endif
  }
  else
  {

#ifdef DEBUG
    Serial.println("Connected!");
#endif
  }
  for (;;)
  {
    wm.process();
    vTaskDelay(1);


    if (WiFi.status() == WL_CONNECTED)
    {
      vTaskDelete(NULL);
    }
  }
}



void getUserInputTask(void *pvParameters)
{
  bool wifiControlEnable = false;
  for (;;)

  {

#ifdef DEBUG
    Serial.print("speedPercentage: ");
    Serial.print(speedPercentage);
    Serial.print(" strokePercentage: ");
    Serial.print(strokePercentage);
    Serial.print(" distance to target: ");
    Serial.println(stepper.getDistanceToTargetSigned());
#endif

    if (digitalRead(WIFI_CONTROL_TOGGLE_PIN) == HIGH)
    {
      if (wifiControlEnable == false)
      {


        wifiControlEnable = true;
        setInternetControl(wifiControlEnable);
      }
      getInternetSettings();

    }
    else
    {
      if (wifiControlEnable == true)
      {


        wifiControlEnable = false;
        setInternetControl(wifiControlEnable);
      }
      speedPercentage = getAnalogAverage(SPEED_POT_PIN, 50);
      strokePercentage = getAnalogAverage(STROKE_POT_PIN, 50);
    }



    if (speedPercentage > minimumCommandPercentage)
    {
      stepper.setSpeedInMillimetersPerSecond(maxSpeedMmPerSecond *
                                             speedPercentage / 100.0);
      stepper.setAccelerationInMillimetersPerSecondPerSecond(
          maxSpeedMmPerSecond * speedPercentage * speedPercentage / accelerationScaling);



    }
    vTaskDelay(100);
  }
}

void motionCommandTask(void *pvParameters)
{

  for (;;)

  {

    while ((stepper.getDistanceToTargetSigned() != 0) ||
           (strokePercentage < minimumCommandPercentage) || (speedPercentage < minimumCommandPercentage))
    {
      vTaskDelay(5);
    }

    float targetPosition = (strokePercentage / 100.0) * maxStrokeLengthMm;
#ifdef DEBUG
    Serial.printf("Moving stepper to position %ld \n", targetPosition);
    vTaskDelay(1);
#endif
    stepper.setDecelerationInMillimetersPerSecondPerSecond(
        maxSpeedMmPerSecond * speedPercentage * speedPercentage / accelerationScaling);
    stepper.setTargetPositionInMillimeters(targetPosition);
    vTaskDelay(1);

    while ((stepper.getDistanceToTargetSigned() != 0) ||
           (strokePercentage < minimumCommandPercentage) || (speedPercentage < minimumCommandPercentage))
    {
      vTaskDelay(5);

    }
    targetPosition = 0;

    vTaskDelay(1);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(
        maxSpeedMmPerSecond * speedPercentage * speedPercentage / accelerationScaling);
    stepper.setTargetPositionInMillimeters(targetPosition);
    vTaskDelay(1);
  }
}

float getAnalogAverage(int pinNumber, int samples)
{
  float sum = 0;
  float average = 0;
  float percentage = 0;
  for (int i = 0; i < samples; i++)
  {

    sum += analogRead(pinNumber);
  }
  average = sum / samples;

  percentage = 100.0 * average / 4096.0;
  return percentage;
}

bool setInternetControl(bool wifiControlEnable)
{




  String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-set-control";



  StaticJsonDocument<200> doc;
  doc["ossmId"] = ossmId;
  doc["wifiControlEnabled"] = wifiControlEnable;
  doc["stroke"] = strokePercentage;
  doc["speed"] = speedPercentage;
  String requestBody;
  serializeJson(doc, requestBody);


  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(requestBody);
  String payload = "{}";
  payload = http.getString();
  http.end();


  StaticJsonDocument<200> bubbleResponse;
  deserializeJson(bubbleResponse, payload);




#ifdef DEBUG
  Serial.print("Setting Wifi Control: ");
  Serial.println(wifiControlEnable);
  Serial.println(requestBody);
  Serial.println(payload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
#endif

  return true;
}

bool getInternetSettings()
{




  String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-get-settings";





  StaticJsonDocument<200> doc;
  doc["ossmId"] = ossmId;
  String requestBody;
  serializeJson(doc, requestBody);


  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(requestBody);
  String payload = "{}";
  payload = http.getString();
  http.end();


  StaticJsonDocument<200> bubbleResponse;
  deserializeJson(bubbleResponse, payload);



  strokePercentage = bubbleResponse["response"]["stroke"];
  speedPercentage = bubbleResponse["response"]["speed"];

#ifdef DEBUG

  Serial.println(payload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
#endif

  return true;
}