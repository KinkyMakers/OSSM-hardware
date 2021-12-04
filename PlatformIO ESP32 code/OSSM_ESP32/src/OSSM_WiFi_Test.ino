#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESP_FlexyStepper.h>
#include <HTTPClient.h>
#include <WiFiManager.h> // https://github.com/tzapu/WiFiManager
#include <Wire.h>
#include "FastLED.h"

// OSSM Reference Remote header files
#include <Encoder.h>
#include "OssmUi.h"

///////////////////////////////////////////
////
////
////  To Debug or not to Debug
////
////
///////////////////////////////////////////

// Uncomment the following line if you wish to print DEBUG info
#define DEBUG

#ifdef DEBUG
#define LogDebug(...) Serial.println(__VA_ARGS__)
#define LogDebugFormatted(...) Serial.printf(__VA_ARGS__)
#else
#define LogDebug(...) ((void)0)
#define LogDebugFormatted(...) ((void)0)
#endif

///////////////////////////////////////////
////
////
////  Things specific to the OSSM Reference Board & Remote
////
////
///////////////////////////////////////////

// SETTINGS

#define ENCODER_SWITCH 35
#define ENCODER_A 18
#define ENCODER_B 5
#define REMOTE_SDA 21
#define REMOTE_CLK 19
#define REMOTE_ADDRESS 0x3c

// CONSTRUCT THINGS

// Homing
volatile bool g_has_not_homed = true;
        bool REMOTE_ATTACHED = false;

// Encoder
Encoder g_encoder(ENCODER_A, ENCODER_B);

// Display
OssmUi g_ui(REMOTE_ADDRESS, REMOTE_SDA, REMOTE_CLK);

///////////////////////////////////////////
////
////
////  Encoder functions & scaling
////
////
///////////////////////////////////////////

IRAM_ATTR void encoderPushButton()
{
    // TODO: Toggle position mode
   // g_encoder.write(0);       // Reset on Button Push
   // g_ui.NextFrame();         // Next Frame on Button Push
    LogDebug("Encoder Button Push");
}

float getEncoderPercentage()
{
    const int encoderFullScale = 100;
    int position = g_encoder.read();
    float positionPercentage;
    if (position < 0)
    {
        g_encoder.write(0);
        position = 0;
    }
    else if (position > encoderFullScale)
    {
        g_encoder.write(encoderFullScale);
        position = encoderFullScale;
    }

    positionPercentage = 100.0 * position / encoderFullScale;

    return positionPercentage;
}

///////////////////////////////////////////
////
////
////  WIFI Management
////
////
///////////////////////////////////////////

// Wifi Manager
WiFiManager wm;

// create the stepper motor object
ESP_FlexyStepper stepper;

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
TaskHandle_t oledTask = nullptr;

#define BRIGHTNESS 170
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define LED_PIN 25
#define NUM_LEDS 1
CRGB leds[NUM_LEDS];

///////////////////////////////////////////
////
////
////  SETUP Parameters
////  These should be moved to an external file only for settings, or much
/// higher up /  it should be immediately obvious where these are and easy for
/// novices to modify
////
////
///////////////////////////////////////////

// Parameters you may need to change for your specific implementation
#define MOTOR_STEP_PIN 14
#define MOTOR_DIRECTION_PIN 27
#define MOTOR_ENABLE_PIN 26
// controller knobs
#define STROKE_POT_PIN 32
#define SPEED_POT_PIN 34
// this pin resets WiFi credentials if needed
#define WIFI_RESET_PIN 0
// this pin toggles between manual knob control and Web-based control
#define WIFI_CONTROL_TOGGLE_PIN 22

#define WIFI_CONTROL_DEFAULT INPUT_PULLDOWN // uncomment for analog pots as default
//#define WIFI_CONTROL_DEFAULT INPUT_PULLUP // uncomment for WiFi control as
// default Pull pin 26 low if you want to switch to analog pot control

// define the IO pin the emergency stop switch is connected to
#define STOP_PIN 19
// define the IO pin where the limit switches are connected to (switches in
// series in normally closed setup)
#define LIMIT_SWITCH_PIN \
    12 // If commented out, limit switch will not be used and toy must be extended
       // at least as far as the "maxStrokeLengthMm"

// limits and physical parameters
const float maxSpeedMmPerSecond = 1000;
const float motorStepPerRevolution = 800;
const float pulleyToothCount = 20;
const float maxStrokeLengthMm = 50; // This is in millimeters, and is what's used to define how much of
                                     // your rail is usable.
//                                            //  150mm on a 400mm rail is
//                                            comfortable and will generally
//                                            avoid smashing endstops
//                                            //  This can be lowered if you
//                                            want to reduce the maximum stroke
const float minStrokeOffLimit = 6; // Machine needs some room away from the limit switch to not tick every
                                   // stroke @ 100% stroke
const float minimumCommandPercentage = 1.0f;
// GT2 belt has 2mm tooth pitch
const float beltPitchMm = 2;

// Tuning parameters
// affects acceleration in stepper trajectory
const float accelerationScaling = 80.0f;

const char *ossmId = "OSSM1"; // this should be unique to your device. You will use this on the
                              // web portal to interact with your OSSM.
// there is NO security other than knowing this name, make this unique to avoid
// collisions with other users

// Declarations
// TODO: Document functions
void setLedRainbow(CRGB leds[]);
void getUserInputTask(void *pvParameters);
void motionCommandTask(void *pvParameters);
void wifiConnectionTask(void *pvParameters);
void estopResetTask(void *pvParameters);
float getAnalogAverage(int pinNumber, int samples);
bool setInternetControl(bool wifiControlEnable);
bool getInternetSettings();

bool stopSwitchTriggered = 0;


/**
 * the iterrupt service routine (ISR) for the emergency swtich
 * this gets called on a rising edge on the IO Pin the emergency switch is
 * connected it only sets the stopSwitchTriggered flag and then returns. The
 * actual emergency stop will than be handled in the loop function
 */
void ICACHE_RAM_ATTR stopSwitchHandler()
{
    stopSwitchTriggered = 1;
    vTaskSuspend(motionTask);
    vTaskSuspend(getInputTask);
    stepper.emergencyStop();
}

///////////////////////////////////////////
////
////
////  VOID SETUP -- Here's where it's hiding
////
////
///////////////////////////////////////////

void setup()
{
    Serial.begin(115200);
    LogDebug("\n Starting");
    delay(200);

    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(150);
    setLedRainbow(leds);
    FastLED.show();
    stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

    float stepsPerMm = motorStepPerRevolution / (pulleyToothCount * beltPitchMm); // GT2 belt has 2mm tooth pitch
    stepper.setStepsPerMillimeter(stepsPerMm);
    // initialize the speed and acceleration rates for the stepper motor. These
    // will be overwritten by user controls. 100 values are placeholders
    stepper.setSpeedInStepsPerSecond(100);
    stepper.setAccelerationInMillimetersPerSecondPerSecond(100);
    stepper.setDecelerationInStepsPerSecondPerSecond(100);
    stepper.setLimitSwitchActive(LIMIT_SWITCH_PIN);

    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
    // put your setup code here, to run once:
    pinMode(MOTOR_ENABLE_PIN, OUTPUT);
    pinMode(WIFI_RESET_PIN, INPUT_PULLUP);
    pinMode(WIFI_CONTROL_TOGGLE_PIN, WIFI_CONTROL_DEFAULT);
    // set the pin for the emegrenxy witch to input with inernal pullup
    // the emergency switch is connected in a Active Low configuraiton in this
    // example, meaning the switch connects the input to ground when closed
    pinMode(STOP_PIN, INPUT_PULLUP);
    // attach an interrupt to the IO pin of the switch and specify the handler
    // function
    attachInterrupt(digitalPinToInterrupt(STOP_PIN), stopSwitchHandler, RISING);
    // Set analog pots (control knobs)
    pinMode(STROKE_POT_PIN, INPUT_PULLUP);
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
        LogDebug("settings reset");
    }

    // OLED SETUP
    g_ui.Setup();
    g_ui.UpdateOnly();
    

    // Rotary Encoder Pushbutton
    pinMode(ENCODER_SWITCH, INPUT_PULLDOWN);
    attachInterrupt(digitalPinToInterrupt(ENCODER_SWITCH), encoderPushButton, RISING);

    if (g_has_not_homed == true)
    {
        LogDebug("OSSM will now home");
        stepper.moveToHomeInMillimeters(1, 100, 400, LIMIT_SWITCH_PIN);
        LogDebug("OSSM has homed, will now move out to max length");
        stepper.moveToPositionInMillimeters((-1 * maxStrokeLengthMm) - minStrokeOffLimit);
        LogDebug("OSSM has moved out, will now set new home?");
        stepper.setCurrentPositionAsHomeAndStop();
        LogDebug("OSSM should now be home and happy");
        g_has_not_homed = false;
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
    xTaskCreatePinnedToCore(wifiConnectionTask,   /* Task function. */
                            "wifiConnectionTask", /* name of task. */
                            10000,                /* Stack size of task */
                            NULL,                 /* parameter of the task */
                            1,                    /* priority of the task */
                            &wifiTask,            /* Task handle to keep track of created task */
                            0);                   /* pin task to core 0 */
    delay(5000);
    xTaskCreatePinnedToCore(getUserInputTask,   /* Task function. */
                            "getUserInputTask", /* name of task. */
                            10000,              /* Stack size of task */
                            NULL,               /* parameter of the task */
                            1,                  /* priority of the task */
                            &getInputTask,      /* Task handle to keep track of created task */
                            0);                 /* pin task to core 0 */
    delay(500);
    xTaskCreatePinnedToCore(motionCommandTask,   /* Task function. */
                            "motionCommandTask", /* name of task. */
                            10000,               /* Stack size of task */
                            NULL,                /* parameter of the task */
                            1,                   /* priority of the task */
                            &motionTask,         /* Task handle to keep track of created task */
                            0);                  /* pin task to core 0 */

    delay(500);
    xTaskCreatePinnedToCore(estopResetTask,   /* Task function. */
                            "estopResetTask", /* name of task. */
                            10000,            /* Stack size of task */
                            NULL,             /* parameter of the task */
                            1,                /* priority of the task */
                            &estopTask,       /* Task handle to keep track of created task */
                            0);               /* pin task to core 0 */

    delay(500);
} // Void Setup()

///////////////////////////////////////////
////
////
////   VOID LOOP - Hides here
////
////
///////////////////////////////////////////

void loop()
{
    g_ui.UpdateState(static_cast<int>(speedPercentage), static_cast<int>(strokePercentage + 0.5f));
    g_ui.UpdateScreen();

    // debug
    static bool is_connected = false;
    if (!is_connected && g_ui.DisplayIsConnected())
    {
        LogDebug("Display Connected");
        is_connected = true;
    }
    else if (is_connected && !g_ui.DisplayIsConnected())
    {
        LogDebug("Display Disconnected");
        is_connected = false;
    }
}

///////////////////////////////////////////
////
////
////  freeRTOS multitasking
////
////
///////////////////////////////////////////

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
        LogDebug("failed to connect and hit timeout");
    }
    else
    {
        // TODO: Set Status LED to indicate everything is ok!
        LogDebug("Connected!");
    }
    for (;;)
    {
        wm.process();
        vTaskDelay(1);

        // delete this task once connected!
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
        //LogDebug("Speed: " + String(speedPercentage) + "\% Stroke: " + String(strokePercentage) +
        //         "\% Distance to target: " + String(stepper.getDistanceToTargetSigned()) + " steps?");



        if (speedPercentage > 1)
        {
            stepper.releaseEmergencyStop();
        }
        else
        {
            stepper.emergencyStop();
            //LogDebug("FULL STOP CAPTAIN");
        }

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
            speedPercentage = getAnalogAverage(SPEED_POT_PIN,
                                               50); // get average analog reading, function takes pin and # samples
            // strokePercentage = getAnalogAverage(STROKE_POT_PIN, 50);
            strokePercentage = getEncoderPercentage();
        }

        // We should scale these values with initialized settings not hard coded
        // values!
        if (speedPercentage > minimumCommandPercentage)
        {
            stepper.setSpeedInMillimetersPerSecond(maxSpeedMmPerSecond * speedPercentage / 100.0);
            stepper.setAccelerationInMillimetersPerSecondPerSecond(maxSpeedMmPerSecond * speedPercentage *
                                                                   speedPercentage / accelerationScaling);
            // We do not set deceleration value here because setting a low decel when
            // going from high to low speed causes the motor to travel a long distance
            // before slowing. We should only change decel at rest
        }
        vTaskDelay(100); // let other code run!
    }
}

void motionCommandTask(void *pvParameters)
{
    for (;;) // tasks should loop forever and not return - or will throw error in
             // OS
    {
        // poll at 200Hz for when motion is complete
        while ((stepper.getDistanceToTargetSigned() != 0) || (strokePercentage < minimumCommandPercentage) ||
               (speedPercentage < minimumCommandPercentage))
        {
            vTaskDelay(5); // wait for motion to complete and requested stroke more than zero
        }

        float targetPosition = (strokePercentage / 100.0) * maxStrokeLengthMm;
        LogDebugFormatted("Moving stepper to position %ld \n", static_cast<long int>(targetPosition));
        vTaskDelay(1);
        stepper.setDecelerationInMillimetersPerSecondPerSecond(maxSpeedMmPerSecond * speedPercentage * speedPercentage /
                                                               accelerationScaling);
        stepper.setTargetPositionInMillimeters(targetPosition);
        vTaskDelay(1);

        while ((stepper.getDistanceToTargetSigned() != 0) || (strokePercentage < minimumCommandPercentage) ||
               (speedPercentage < minimumCommandPercentage))
        {
            vTaskDelay(5); // wait for motion to complete, since we are going back to
                           // zero, don't care about stroke value
        }
        targetPosition = 0;
        // Serial.printf("Moving stepper to position %ld \n", targetPosition);
        vTaskDelay(1);
        stepper.setDecelerationInMillimetersPerSecondPerSecond(maxSpeedMmPerSecond * speedPercentage * speedPercentage /
                                                               accelerationScaling);
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

    String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-set-control"; // live server
    // String serverNameBubble =
    // "http://d2oq8yqnezqh3r.cloudfront.net/ossm-set-control"; // this is
    // version-test server

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

    const char *wifiEnabledStr = (wifiControlEnable ? "true" : "false");
    LogDebugFormatted("Setting Wifi Control: %s\n%s\n%s\n", wifiEnabledStr, requestBody.c_str(), payload.c_str());
    LogDebugFormatted("HTTP Response code: %d\n", httpResponseCode);

    return true;
}

bool getInternetSettings()
{
    // here we will request speed and stroke settings from the remote server. The
    // cloudfront redirect allows http connection with bubble backend hosted at
    // app.researchanddesire.com

    String serverNameBubble = "http://d2g4f7zewm360.cloudfront.net/ossm-get-settings"; // live server
    // String serverNameBubble =
    // "http://d2oq8yqnezqh3r.cloudfront.net/ossm-get-settings"; // this is
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

    // debug info on the http payload
    LogDebug(payload);
    LogDebugFormatted("HTTP Response code: %d\n", httpResponseCode);

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
