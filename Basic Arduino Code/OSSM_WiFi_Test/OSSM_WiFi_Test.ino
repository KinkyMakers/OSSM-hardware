#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP_FlexyStepper.h>

#define TRIGGER_PIN 0
int timeout = 120; // seconds to run for
long strokePercentage = 0;
long speedPercentage = 0;
bool wifiControlEnable = true;
char ossmId[20] = "OSSM1";
WiFiManager wm;

// IO pin assignments
const int MOTOR_STEP_PIN = 27;
const int MOTOR_DIRECTION_PIN = 25;
const int EMERGENCY_STOP_PIN = 13; //define the IO pin the emergency stop switch is connected to
const int LIMIT_SWITCH_PIN = 27;   //define the IO pin where the limit switches are connected to (switches in series in normally closed setup against ground)

// Speed settings
const int DISTANCE_TO_TRAVEL_IN_STEPS = 2000;
const int SPEED_IN_STEPS_PER_SECOND = 300;
const int ACCELERATION_IN_STEPS_PER_SECOND = 800;
const int DECELERATION_IN_STEPS_PER_SECOND = 800;

// create the stepper motor object
ESP_FlexyStepper stepper;

TaskHandle_t httpTask;
TaskHandle_t motionTask;

int previousDirection = 1;

void setup()
{
  Serial.begin(115200);
  Serial.println("\n Starting");
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);
  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepper.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);

  // Not start the stepper instance as a service in the "background" as a separate task
  // and the OS of the ESP will take care of invoking the processMovement() task regularily so you can do whatever you want in the loop function
  stepper.startAsService();

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:

  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  // is configuration portal requested?
  if (digitalRead(TRIGGER_PIN) == LOW)
  {

    //reset settings - for testing
    wm.resetSettings();

    //if you get here you have connected to the WiFi
    Serial.println("settings reset");
  }
  if (!wm.autoConnect("OSSM-setup"))
  {
    Serial.println("failed to connect and hit timeout");
  }
  else
  {
    Serial.println("Connected!");
  }
  setInternetControl();

  xTaskCreatePinnedToCore(
      httpTaskcode, /* Task function. */
      "httpTask",   /* name of task. */
      10000,        /* Stack size of task */
      NULL,         /* parameter of the task */
      1,            /* priority of the task */
      &httpTask,    /* Task handle to keep track of created task */
      0);           /* pin task to core 0 */
  delay(500);
  xTaskCreatePinnedToCore(
      motionTaskcode, /* Task function. */
      "motionTask",   /* name of task. */
      10000,          /* Stack size of task */
      NULL,           /* parameter of the task */
      1,              /* priority of the task */
      &motionTask,    /* Task handle to keep track of created task */
      0);             /* pin task to core 0 */
  delay(500);
}
void httpTaskcode(void *pvParameters)
{
  for (;;)
  {
    Serial.print("Task1 running on core ");
    Serial.println(xPortGetCoreID());
    getInternetSettings();
    delay(100);
  }
}
void motionTaskcode(void *pvParameters)
{
  for (;;)
  {

    stepper.setSpeedInStepsPerSecond(200.0 * speedPercentage);
    stepper.setAccelerationInStepsPerSecondPerSecond(20.0 * speedPercentage * speedPercentage);
    stepper.setDecelerationInStepsPerSecondPerSecond(20.0 * speedPercentage * speedPercentage);
    if (stepper.getDistanceToTargetSigned() == 0)
    {
      delay(10);
      Serial.print("loop running on core ");
      Serial.println(xPortGetCoreID());
      previousDirection *= -1;
      long relativeTargetPosition = strokePercentage * 200 * previousDirection;
      Serial.printf("Moving stepper by %ld steps\n", relativeTargetPosition);
      stepper.setTargetPositionRelativeInSteps(relativeTargetPosition);
    }

    // for (int i = 0; i < 101; i++)
    // {
    //   strokePercentage = i;
    //   speedPercentage = i / 2;
    //   wifiControlEnable = ((i % 5) == 0);
    //   setInternetControl();
    //   getInternetSettings();
    //   Serial.print("Stroke: ");
    //   Serial.print(strokePercentage);
    //   Serial.print("  Speed: ");
    //   Serial.println(speedPercentage);
    // }

    // put your main code here, to run repeatedly:
  }
}

void loop()
{
}

bool setInternetControl()
{

  //String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-set-control"; //live
  String serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/ossm-set-control"; //this is version-test server
  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  // Add values in the document
  doc["ossmId"] = ossmId;
  doc["wifiControlEnabled"] = wifiControlEnable;
  doc["stroke"] = strokePercentage;
  doc["speed"] = speedPercentage;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  String payload = "{}";
  payload = http.getString();
  StaticJsonDocument<200> bubbleResponse;

  deserializeJson(bubbleResponse, payload);

  const char *status = bubbleResponse["status"]; // "success"

  Serial.println(payload);

  // display.display();
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  // delay(1000);

  http.end();
  return true;
}

bool getInternetSettings()
{

  //String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-get-settings"; //live
  String serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/ossm-get-settings"; //this is version-test server
  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  // Add values in the document
  doc["ossmId"] = ossmId;

  String requestBody;
  serializeJson(doc, requestBody);

  int httpResponseCode = http.POST(requestBody);

  String payload = "{}";
  //int httpResponseCode = http.POST("{\"trainerSwVersion\":\"96\"}");
  payload = http.getString();
  StaticJsonDocument<200> bubbleResponse;

  deserializeJson(bubbleResponse, payload);

  const char *status = bubbleResponse["status"]; // "success"
  strokePercentage = bubbleResponse["response"]["stroke"];
  speedPercentage = bubbleResponse["response"]["speed"];

  //strcpy(username, bubbleResponse["response"]["username"]);

  Serial.println(payload);

  // display.display();
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  // delay(1000);

  http.end();
  return true;
}
