#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <ESP_FlexyStepper.h>

#define TRIGGER_PIN 0 //this pin resets WiFi credentials if needed
float strokePercentage = 0;
float speedPercentage = 0;
float targetPosition = 0;
float deceleration;
WiFiManager wm;

// Speed settings - this will be changed by pot input or wifi settings
const int DISTANCE_TO_TRAVEL_IN_MM = 2000;
const int SPEED_IN_MM_PER_SECOND = 300;
const int ACCELERATION_IN_MM_PER_SECOND = 800;
const float MAX_SPEED_MM_PER_SECOND = 600;

// create the stepper motor object
ESP_FlexyStepper stepper;

//Create tasks for checking pot input or web server control, and task to handle planning the motion profile (this task is high level only and does not pulse the stepper!)
TaskHandle_t getInputTask;
TaskHandle_t motionTask;

// Parameters you may need to change for your specific implementation
const int MOTOR_STEP_PIN = 27;
const int MOTOR_DIRECTION_PIN = 25;
const int WIFI_CONTROL_TOGGLE_PIN = 26;
const int STROKE_POT_PIN = 32;
const int SPEED_POT_PIN = 33;
const float motorStepPerRevolution = 800;
const float pulleyToothCount = 20;
const float strokeLength = 150;

const int EMERGENCY_STOP_PIN = 22; //define the IO pin the emergency stop switch is connected to
const int LIMIT_SWITCH_PIN = 21;   //define the IO pin where the limit switches are connected to (switches in series in normally closed setup)
char ossmId[20] = "OSSM1";         // this should be unique to your device. You will use this on the web portal to interact with your OSSM.
//there is NO security other than knowing this name, make this unique to avoid collisions with other users

float stepsPerMm = motorStepPerRevolution / (pulleyToothCount * 2); //GT2 belt has 2mm tooth pitch

void setup()
{
  Serial.begin(115200);
  Serial.println("\n Starting");

  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  stepper.setStepsPerMillimeter(stepsPerMm);
  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_MM_PER_SECOND);
  stepper.setAccelerationInMillimetersPerSecondPerSecond(ACCELERATION_IN_MM_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(ACCELERATION_IN_MM_PER_SECOND);

  // Not start the stepper instance as a service in the "background" as a separate task
  // and the OS of the ESP will take care of invoking the processMovement() task regularily so you can do whatever you want in the loop function
  stepper.startAsService(); // Kinky Makers - we have modified this function from default library to run on core 1 and suggest you don't run anything else on that core.

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:
  pinMode(TRIGGER_PIN, INPUT_PULLUP);
  pinMode(WIFI_CONTROL_TOGGLE_PIN, INPUT_PULLDOWN);
  pinMode(STROKE_POT_PIN, INPUT);
  adcAttachPin(STROKE_POT_PIN);
  pinMode(SPEED_POT_PIN, INPUT);
  adcAttachPin(SPEED_POT_PIN);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); //allows us to read almost full 3.3V range

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
    Serial.println(digitalRead(WIFI_CONTROL_TOGGLE_PIN));
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
      Serial.print("speedPercentage: ");
      Serial.print(speedPercentage);
      Serial.print(" strokePercentage: ");
      Serial.print(strokePercentage);
      Serial.print(" distance to target: ");
      Serial.print(stepper.getDistanceToTargetSigned());

      if (wifiControlEnable == true)
      {
        //this is a transition to local control, we should tell the server it cannot control
        wifiControlEnable = false;
        setInternetControl(wifiControlEnable);
      }
      speedPercentage = getAnalogAverage(SPEED_POT_PIN, 50); //get average analog reading, function takes pin and # samples
      strokePercentage = getAnalogAverage(STROKE_POT_PIN, 50);
    }

    //We should scale these values with initialized settings not hard coded values!
    if (speedPercentage>1){
    //deceleration needs work
    stepper.setSpeedInMillimetersPerSecond(MAX_SPEED_MM_PER_SECOND * speedPercentage / 100.0);
    stepper.setAccelerationInMillimetersPerSecondPerSecond(MAX_SPEED_MM_PER_SECOND * speedPercentage * speedPercentage / 80.0);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(MAX_SPEED_MM_PER_SECOND * speedPercentage * speedPercentage / 80.0);
    }
    vTaskDelay(100); //let other code run!
  }
}
void motionTaskcode(void *pvParameters)
{
  for (;;) //tasks should loop forever and not return - or will throw error in OS
  {
    while ((stepper.getDistanceToTargetSigned() != 0) || (strokePercentage < 1) || (speedPercentage < 1))
    {
      Serial.print("waiting to go to out stroke ");
      vTaskDelay(5); //wait for motion to complete and requested stroke more than zero
    }
    targetPosition = (strokePercentage / 100.0) * strokeLength;
    // Serial.printf("Moving stepper to position %ld \n", targetPosition);
    vTaskDelay(1);
    stepper.setTargetPositionInMillimeters(targetPosition);
    vTaskDelay(5);

    while ((stepper.getDistanceToTargetSigned() != 0) || (strokePercentage < 1) || (speedPercentage < 1))
    {
      Serial.print("waiting to go to home ");
      vTaskDelay(5); //wait for motion to complete, since we are going back to zero, don't care about stroke value
    }
    targetPosition = 0;
     Serial.printf("Moving stepper to position %ld \n", targetPosition);
    vTaskDelay(1);
    stepper.setTargetPositionInMillimeters(targetPosition);
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
  float percentage = 0;
  for (int i = 0; i < samples; i++)
  {
    sum += analogRead(pinNumber);
  }
  average = sum / samples;
  percentage = 100.0 * average / 4096.0; //12 bit resolution
  return percentage;
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
