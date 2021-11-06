#include <ArduinoJson.h>
#include <ESP_FlexyStepper.h>
#include <HTTPClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <FastLED.h>
#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>
#include <Adafruit_I2CDevice.h>
#include <ESP32Servo.h>

// Wifi Manager
WiFiManager wm;

// create the stepper motor object
ESP_FlexyStepper stepper;

#define BRIGHTNESS 170
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define LED_PIN 25
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
#define I2C_SDA 21
#define I2C_SCL 19
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define SCREEN_ADDRESS 0x3C
#define OLED_RESET 0
#define RXD2 16
#define TXD2 17
int servo_output = 4;

TwoWire I2CREMOTE = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2CREMOTE, OLED_RESET);

// Current command state
volatile float strokePercentage = 0;
volatile float speedPercentage = 0;
volatile float deceleration = 0;

// Create tasks for checking pot input or web server control, and task to handle
// planning the motion profile (this task is high level only and does not pulse
// the stepper!)
TaskHandle_t wifiTask = nullptr;
TaskHandle_t getInputTask = nullptr;
TaskHandle_t motionTask = nullptr;
TaskHandle_t estopTask = nullptr;

// Parameters you may need to change for your specific implementation
#define MOTOR_STEP_PIN 14
#define MOTOR_DIRECTION_PIN 27
#define MOTOR_ENABLE_PIN 26
// controller knobs
#define STROKE_POT_PIN 32
#define SPEED_POT_PIN 33
// this pin resets WiFi credentials if needed
#define WIFI_RESET_PIN 0
// this pin toggles between manual knob control and Web-based control
#define WIFI_CONTROL_TOGGLE_PIN 22

//#define WIFI_CONTROL_DEFAULT INPUT_PULLDOWN // uncomment for analog pots as default
#define WIFI_CONTROL_DEFAULT INPUT_PULLUP // uncomment for WiFi control as default
//Pull pin 26 low if you want to switch to analog pot control

// define the IO pin the emergency stop switch is connected to
#define STOP_PIN 19
// define the IO pin where the limit switches are connected to (switches in
// series in normally closed setup)
#define LIMIT_SWITCH_PIN 21

#define DEBUG

// limits and physical parameters
const float maxSpeedMmPerSecond = 10000;
const float motorStepPerRevolution = 1600;
const float pulleyToothCount = 20;
const float maxStrokeLengthMm = 1500;
const float minimumCommandPercentage = 1.0f;
// GT2 belt has 2mm tooth pitch
const float beltPitchMm = 2;

// Tuning parameters
// affects acceleration in stepper trajectory
const float accelerationScaling = 80.0f;

const char *ossmId = "OSSM123"; // this should be unique to your device. You will use this on the
                                // web portal to interact with your OSSM.
// there is NO security other than knowing this name, make this unique to avoid
// collisions with other users

// Declarations
// TODO: Document functions
void getUserInputTask(void *pvParameters);
void motionCommandTask(void *pvParameters);
float getAnalogAverage(int pinNumber, int samples);
bool setInternetControl(bool wifiControlEnable);
bool getInternetSettings();

bool stopSwitchTriggered = 0;


/**
 * the iterrupt service routine (ISR) for the emergency swtich
 * this gets called on a rising edge on the IO Pin the emergency switch is connected
 * it only sets the stopSwitchTriggered flag and then returns. 
 * The actual emergency stop will than be handled in the loop function
 */
void ICACHE_RAM_ATTR stopSwitchHandler()
{
  stopSwitchTriggered = 1;
  vTaskSuspend(motionTask);
  vTaskSuspend(getInputTask);
  stepper.emergencyStop();
}

void setup()
{
  pinMode(5, INPUT_PULLDOWN);
  pinMode(18, INPUT_PULLDOWN);
  delay(10000);
  

  HardwareSerial Serial2(2);
  Serial.begin(57600);
  Serial2.begin(57600, SERIAL_8N1, 16, 17);

  // while (true)
  // {
  //   if (Serial2.available())
  //   {
  //     Serial.print(char(Serial2.read()));
  //   }
  //   if (Serial.available())
  //   {
  //     Serial2.print(char(Serial.read()));
  //   }
  // }
#ifdef DEBUG
  Serial.println("\n Starting");
  delay(200);
#endif
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(150);
  setLedRainbow(leds);
  FastLED.show();
  I2CREMOTE.begin(I2C_SDA, I2C_SCL, 1000);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();


  // Clear the buffer
  display.clearDisplay();
  display.setTextColor(WHITE); // Draw white text
  display.setTextWrap(false);
  display.setTextSize(1); // Normal 1:1 pixel scale
  display.setCursor(0, 0);
  display.print("HELLO");
  display.display();  
  delay(2000); // Pause for 2 seconds

  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  float stepsPerMm =
      motorStepPerRevolution /
      (pulleyToothCount * beltPitchMm); // GT2 belt has 2mm tooth pitch
  stepper.setStepsPerMillimeter(stepsPerMm);
  // initialize the speed and acceleration rates for the stepper motor. These
  // will be overwritten by user controls. 100 values are placeholders
  stepper.setSpeedInStepsPerSecond(100);
  stepper.setAccelerationInMillimetersPerSecondPerSecond(100);
  stepper.setDecelerationInStepsPerSecondPerSecond(100);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  // put your setup code here, to run once:
  pinMode(MOTOR_ENABLE_PIN, OUTPUT);
  pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
  pinMode(WIFI_CONTROL_TOGGLE_PIN, WIFI_CONTROL_DEFAULT);
  //set the pin for the emegrenxy witch to input with inernal pullup
  //the emergency switch is connected in a Active Low configuraiton in this example, meaning the switch connects the input to ground when closed
  pinMode(STOP_PIN, INPUT_PULLUP);
  //attach an interrupt to the IO pin of the switch and specify the handler function
  attachInterrupt(digitalPinToInterrupt(STOP_PIN), stopSwitchHandler, RISING);
  // Set analog pots (control knobs)
  pinMode(STROKE_POT_PIN, INPUT);
  adcAttachPin(STROKE_POT_PIN);

  pinMode(SPEED_POT_PIN, INPUT);
  adcAttachPin(SPEED_POT_PIN);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // allows us to read almost full 3.3V range

  // This is here in case you want to change WiFi settings - pull IO low
  if (digitalRead(WIFI_RESET_PIN) == LOW)
  {
    // reset settings - for testing
    wm.resetSettings();
#ifdef DEBUG
    Serial.println("settings reset");
#endif
  }

  // Start the stepper instance as a service in the "background" as a separate
  // task and the OS of the ESP will take care of invoking the processMovement()
  // task regularly on core 1 so you can do whatever you want on core 0
  stepper.startAsService(); // Kinky Makers - we have modified this function
                            // from default library to run on core 1 and suggest
                            // you don't run anything else on that core.

  // Kick off the http and motion tasks - they begin executing as soon as they
  // are created here! Do not change the priority of the task, or do so with
  // caution. RTOS runs first in first out, so if there are no delays in your
  // tasks they will prevent all other code from running on that core!
  xTaskCreatePinnedToCore(
      wifiConnectionTask,   /* Task function. */
      "wifiConnectionTask", /* name of task. */
      10000,                /* Stack size of task */
      NULL,                 /* parameter of the task */
      1,                    /* priority of the task */
      &wifiTask,            /* Task handle to keep track of created task */
      0);                   /* pin task to core 0 */
  delay(5000);
  xTaskCreatePinnedToCore(
      getUserInputTask,   /* Task function. */
      "getUserInputTask", /* name of task. */
      10000,              /* Stack size of task */
      NULL,               /* parameter of the task */
      1,                  /* priority of the task */
      &getInputTask,      /* Task handle to keep track of created task */
      0);                 /* pin task to core 0 */
  delay(500);
  xTaskCreatePinnedToCore(
      motionCommandTask,   /* Task function. */
      "motionCommandTask", /* name of task. */
      10000,               /* Stack size of task */
      NULL,                /* parameter of the task */
      1,                   /* priority of the task */
      &motionTask,         /* Task handle to keep track of created task */
      0);                  /* pin task to core 0 */

  delay(500);
  xTaskCreatePinnedToCore(
      estopResetTask,   /* Task function. */
      "estopResetTask", /* name of task. */
      10000,            /* Stack size of task */
      NULL,             /* parameter of the task */
      1,                /* priority of the task */
      &estopTask,       /* Task handle to keep track of created task */
      0);               /* pin task to core 0 */

  delay(500);
}

void loop()
{
  vTaskDelete(NULL); // we don't want this loop to run (because it runs on core
                     // 0 where we have the critical FlexyStepper code)
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
  // here we try to connect to WiFi or launch settings hotspot for you to enter
  // WiFi credentials
  if (!wm.autoConnect("OSSM-setup"))
  {
    // TODO: Set Status LED to indicate failure
#ifdef DEBUG
    Serial.println("failed to connect and hit timeout");
#endif
  }
  else
  {
    // TODO: Set Status LED to indicate everything is ok!
#ifdef DEBUG
    Serial.println("Connected!");
#endif
  }
  for (;;)
  {
    wm.process();
    vTaskDelay(1);

    //delete this task once connected!
    if (WiFi.status() == WL_CONNECTED)
    {
      vTaskDelete(NULL);
    }
  }
}

// Task to read settings from server - only need to check this when in WiFi
// control mode
void getUserInputTask(void *pvParameters)
{
  bool wifiControlEnable = false;
  for (;;) // tasks should loop forever and not return - or will throw error in
           // OS
  {

#ifdef DEBUG
    Serial.print("speedPercentage: ");
    Serial.print(speedPercentage);
    Serial.print(" strokePercentage: ");
    Serial.print(strokePercentage);
    Serial.print(" distance to target: ");
    Serial.println(stepper.getDistanceToTargetSigned());
#endif

    if (digitalRead(WIFI_CONTROL_TOGGLE_PIN) == HIGH) // TODO: check if wifi available and handle gracefully
    {
      if (wifiControlEnable == false)
      {
        // this is a transition to WiFi, we should tell the server it has
        // control
        wifiControlEnable = true;
        setInternetControl(wifiControlEnable);
      }
      getInternetSettings(); // we load speedPercentage and strokePercentage in
                             // this routine.
    }
    else
    {
      if (wifiControlEnable == true)
      {
        // this is a transition to local control, we should tell the server it
        // cannot control
        wifiControlEnable = false;
        setInternetControl(wifiControlEnable);
      }
      speedPercentage = getAnalogAverage(SPEED_POT_PIN, 50); // get average analog reading, function takes pin and # samples
      strokePercentage = getAnalogAverage(STROKE_POT_PIN, 50);
    }

    // We should scale these values with initialized settings not hard coded
    // values!
    if (speedPercentage > minimumCommandPercentage)
    {
      stepper.setSpeedInMillimetersPerSecond(maxSpeedMmPerSecond *
                                             speedPercentage / 100.0);
      stepper.setAccelerationInMillimetersPerSecondPerSecond(
          maxSpeedMmPerSecond * speedPercentage * speedPercentage / accelerationScaling);
      // We do not set deceleration value here because setting a low decel when
      // going from high to low speed causes the motor to travel a long distance
      // before slowing. We should only change decel at rest
    }
    vTaskDelay(100); // let other code run!
  }
}

void motionCommandTask(void *pvParameters)
{
  int pos = 0;
  const int lowerHand = 0;
  int raiseHand = 200;
  Servo myservo;
  myservo.setPeriodHertz(50);               // standard 50 hz servo
  myservo.attach(servo_output, 1000, 2000); // attaches the servo on pin 18 to the servo object
                                            // using default min/max of 1000us and 2000us
  myservo.write(lowerHand);

  for (;;) // tasks should loop forever and not return - or will throw error in
           // OS
  {
    // poll at 200Hz for when motion is complete
    while ((stepper.getDistanceToTargetSigned() != 0) ||
           (strokePercentage < minimumCommandPercentage) || (speedPercentage < minimumCommandPercentage))
    {
      vTaskDelay(5); // wait for motion to complete and requested stroke more than zero
    }
    for (pos = raiseHand; pos >= lowerHand; pos -= 1)
    { // goes from 180 degrees to 0 degrees
      // in steps of 1 degree
      myservo.write(pos); // tell servo to go to position in variable 'pos'
      delay(1);           // waits 15ms for the servo to reach the position
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
      vTaskDelay(5); // wait for motion to complete, since we are going back to
                     // zero, don't care about stroke value
    }
    for (pos = lowerHand; pos <= raiseHand; pos += 1)
    { // goes from 180 degrees to 0 degrees
      // in steps of 1 degree
      myservo.write(pos); // tell servo to go to position in variable 'pos'
      delay(1);           // waits 15ms for the servo to reach the position
    }

    targetPosition = 0;
    // Serial.printf("Moving stepper to position %ld \n", targetPosition);
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
    // TODO: Possibly use fancier filters?
    sum += analogRead(pinNumber);
  }
  average = sum / samples;
  // TODO: Might want to add a deadband
  percentage = 100.0 * average / 4096.0; // 12 bit resolution
  return percentage;
}

bool setInternetControl(bool wifiControlEnable)
{
  // here we will SEND the WiFi control permission, and current speed and stroke
  // to the remote server. The cloudfront redirect allows http connection with
  // bubble backend hosted at app.researchanddesire.com

  String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-set-control"; //live server
  // String serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/ossm-set-control"; // this is version-test server

  // Add values in the document to send to server
  StaticJsonDocument<200> doc;
  doc["ossmId"] = ossmId;
  doc["wifiControlEnabled"] = wifiControlEnable;
  doc["stroke"] = strokePercentage;
  doc["speed"] = speedPercentage;
  String requestBody;
  serializeJson(doc, requestBody);

  // Http request
  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");
  // post and wait for response
  int httpResponseCode = http.POST(requestBody);
  String payload = "{}";
  payload = http.getString();
  http.end();

  // deserialize JSON
  StaticJsonDocument<200> bubbleResponse;
  deserializeJson(bubbleResponse, payload);

  // TODO: handle status response
  // const char *status = bubbleResponse["status"]; // "success"

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
  // here we will request speed and stroke settings from the remote server. The
  // cloudfront redirect allows http connection with bubble backend hosted at
  // app.researchanddesire.com

  String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-get-settings"; //live server
  // String serverNameBubble = "http://d2oq8yqnezqh3r.cloudfront.net/ossm-get-settings"; // this is
  // version-test
  // server

  // Add values in the document
  StaticJsonDocument<200> doc;
  doc["ossmId"] = ossmId;
  String requestBody;
  serializeJson(doc, requestBody);

  // Http request
  HTTPClient http;
  http.begin(serverNameBubble);
  http.addHeader("Content-Type", "application/json");
  // post and wait for response
  int httpResponseCode = http.POST(requestBody);
  String payload = "{}";
  payload = http.getString();
  http.end();

  // deserialize JSON
  StaticJsonDocument<200> bubbleResponse;
  deserializeJson(bubbleResponse, payload);

  // TODO: handle status response
  // const char *status = bubbleResponse["status"]; // "success"
  strokePercentage = bubbleResponse["response"]["stroke"];
  speedPercentage = bubbleResponse["response"]["speed"];

#ifdef DEBUG
  // debug info on the http payload
  Serial.println(payload);
  Serial.print("HTTP Response code: ");
  Serial.println(httpResponseCode);
#endif

  return true;
}

void setLedRainbow(CRGB leds[])
{
  // int power = 250;

  for (int hueShift = 0; hueShift < 350; hueShift++)
  {
    int gHue = hueShift % 255;
    fill_rainbow(leds, NUM_LEDS, gHue, 25);
    FastLED.show();
    delay(4);
  }
}