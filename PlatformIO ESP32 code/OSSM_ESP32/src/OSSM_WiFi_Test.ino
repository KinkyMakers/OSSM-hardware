#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP_FlexyStepper.h>

#define TRIGGER_PIN 0 //this pin resets WiFi credentials if needed
long strokePercentage = 0;
long speedPercentage = 0;
int previousDirection = 1;
WiFiManager wm;

// Speed settings - this will be changed by pot input or wifi settings
const int DISTANCE_TO_TRAVEL_IN_STEPS = 2000;
const int SPEED_IN_STEPS_PER_SECOND = 300;
const int ACCELERATION_IN_STEPS_PER_SECOND = 800;
const int DECELERATION_IN_STEPS_PER_SECOND = 800;

// create the stepper motor object
ESP_FlexyStepper stepper;

//Create tasks for http calls to server and task to handle planning the motion profile (this task is high level only and does not pulse the stepper!)
TaskHandle_t getInputTask;
TaskHandle_t motionTask;

// Parameters you may need to change for your specific implementation
const int MOTOR_STEP_PIN = 27;
const int MOTOR_DIRECTION_PIN = 25;
const int WIFI_CONTROL_TOGGLE_PIN = 26;
const int STROKE_POT_PIN = 32;
const int SPEED_POT_PIN = 33;

const int EMERGENCY_STOP_PIN = 22; //define the IO pin the emergency stop switch is connected to
const int LIMIT_SWITCH_PIN = 21;   //define the IO pin where the limit switches are connected to (switches in series in normally closed setup)
char ossmId[20] = "OSSM1";         // this should be unique to your device. You will use this on the web portal to interact with your OSSM.
//there is NO security other than knowing this name, make this unique to avoid collisions with other users

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
  stepper.startAsService(); // Kinky Makers - we have modified this function from default library to run on core 1 and suggest you don't run anything else on that core.

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:
  pinMode(TRIGGER_PIN, INPUT_PULLUP);

  // This is here in case you want to change WiFi settings - pull IO low
  if (digitalRead(TRIGGER_PIN) == LOW)
  {
    //reset settings - for testing
    wm.resetSettings();
    Serial.println("settings reset");
  }

  //here we try to connect to WiFi or launch settings hotspot for you to enter WiFi credentials
  if (!wm.autoConnect("OSSM-setup"))
  {
    Serial.println("failed to connect and hit timeout");
  }
  else
  {
    Serial.println("Connected!");
  }

  //Kick off the http and motion tasks - they begin executing as soon as they are created here! Do not change the priority of the task, or do so with caution.
  //RTOS runs first in first out, so if there are no delays in your tasks they will prevent all other code from running on that core!
  xTaskCreatePinnedToCore(
      getInputTaskcode, /* Task function. */
      "getInputTask",   /* name of task. */
      10000,            /* Stack size of task */
      NULL,             /* parameter of the task */
      1,                /* priority of the task */
      &getInputTask,    /* Task handle to keep track of created task */
      0);               /* pin task to core 0 */
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

//Task to read settings from server - only need to check this when in WiFi control mode
void getInputTaskcode(void *pvParameters)
{
  bool wifiControlEnable = false;
  for (;;) //tasks should loop forever and not return - or will throw error in OS
  {
    if (digitalRead(WIFI_CONTROL_TOGGLE_PIN) == HIGH)
    {
      if (wifiControlEnable == false)
      {
        //this is a transition to WiFi, we should tell the server it has control
        wifiControlEnable = true;
        setInternetControl(wifiControlEnable);
      }
      getInternetSettings(); //we load speedPercentage and strokePercentage in this routine.
    }
    else
    {
      if (wifiControlEnable == false)
      {
        //this is a transition to local control, we should tell the server it cannot control
        wifiControlEnable = false;
        setInternetControl(wifiControlEnable);
      }
      speedPercentage = getAnalogAverage(SPEED_POT_PIN,50); //get average analog reading, function takes pin and # samples
      strokePercentage = getAnalogAverage(STROKE_POT_PIN,50);
    }

    //We should scale these values with initialized settings not hard coded values!
    stepper.setSpeedInStepsPerSecond(600.0 * speedPercentage);
    stepper.setAccelerationInStepsPerSecondPerSecond(30.0 * speedPercentage * speedPercentage);
    stepper.setDecelerationInStepsPerSecondPerSecond(30.0 * speedPercentage * speedPercentage);
    vTaskDelay(100); //let other code run!
  }
}
void motionTaskcode(void *pvParameters)
{
  for (;;) //tasks should loop forever and not return - or will throw error in OS
  {
    if (stepper.getDistanceToTargetSigned() == 0)
    {
      vTaskDelay(1);
      Serial.print("loop running on core ");
      Serial.println(xPortGetCoreID());
      previousDirection *= -1;
      long relativeTargetPosition = strokePercentage * 30 * previousDirection;
      Serial.printf("Moving stepper by %ld steps\n", relativeTargetPosition);
      stepper.setTargetPositionRelativeInSteps(relativeTargetPosition);
    }
    vTaskDelay(5);
  }
}

void loop()
{
  vTaskDelete(NULL); //we don't want this loop to run (because it runs on core 0 where we have the critical FlexyStepper code)
}

float getAnalogAverage(int pinNumber, int samples)
{
  float sum = 0;
  float average = 0;
  for (int i = 0; i++; i < samples)
  {
    sum += analogRead(pinNumber);
  }
  average = sum / samples;
  return average;
}

bool setInternetControl(bool wifiControlEnable)
{
  //here we will SEND the WiFi control permission, and current speed and stroke to the remote server.
  //The cloudfront redirect allows http connection with bubble backend hosted at app.researchanddesire.com

  //String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-set-control"; //live server
  String serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/ossm-set-control"; //this is version-test server
  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<200> doc;
  // Add values in the document to send to server
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
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
  return true;
}

bool getInternetSettings()
{
  //here we will request speed and stroke settings from the remote server. The cloudfront redirect allows http connection with bubble backend hosted at app.researchanddesire.com

  //String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-get-settings"; //live server
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
  payload = http.getString();
  StaticJsonDocument<200> bubbleResponse;

  deserializeJson(bubbleResponse, payload);

  const char *status = bubbleResponse["status"]; // "success"
  strokePercentage = bubbleResponse["response"]["stroke"];
  speedPercentage = bubbleResponse["response"]["speed"];

  //debug info on the http payload
  Serial.println(payload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
  http.end();
  return true;
}
