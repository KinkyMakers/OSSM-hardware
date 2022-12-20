#include <Arduino.h>          // Basic Needs
#include <ArduinoJson.h>      // Needed for the Bubble APP
#include <ESP_FlexyStepper.h> // Current Motion Control
#include <Encoder.h>          // Used for the Remote Encoder Input
#include <HTTPClient.h>       // Needed for the Bubble APP
#include <WiFiManager.h>      // Used to provide easy network connection  https://github.com/tzapu/WiFiManager
#include <Wire.h>             // Used for i2c connections (Remote OLED Screen)

#include "OSSM_Config.h" // START HERE FOR Configuration
#include "OSSM_PinDef.h" // This is where you set pins specific for your board
#include "OssmUi.h"      // Separate file that helps contain the OLED screen functions
#include "Stroke_Engine_Helper.h"
#include "Utilities.h" // Utility helper functions - wifi update and homing

// Homing
volatile bool g_has_not_homed = true;
bool REMOTE_ATTACHED = false;

// OSSM name setup
const char *ossmId = "OSSM1";
volatile int encoderButtonPresses = 0; // increment for each click
volatile long lastEncoderButtonPressMillis = 0;

IRAM_ATTR void encoderPushButton()
{
    // TODO: Toggle position mode
    // g_encoder.write(0);       // Reset on Button Push
    // ossm.g_ui.NextFrame();         // Next Frame on Button Push

    // debounce check
    long currentTime = millis();
    if ((currentTime - lastEncoderButtonPressMillis) > 200)
    {
        // run interrupt if not run in last 50ms
        encoderButtonPresses++;
        lastEncoderButtonPressMillis = currentTime;
    }
}

// Current command state
// volatile float strokePercentage = 0;
// volatile float ossm.speedPercentage = 0;
// volatile float deceleration = 0;

// Create tasks for checking pot input or web server control, and task to handle
// planning the motion profile (this task is high level only and does not pulse
// the stepper!)
TaskHandle_t wifiTask = nullptr;
TaskHandle_t getInputTask = nullptr;
TaskHandle_t motionTask = nullptr;
TaskHandle_t estopTask = nullptr;
TaskHandle_t oledTask = nullptr;

// Declarations
// TODO: Document functions
// void setLedRainbow(CRGB leds[]);
void getUserInputTask(void *pvParameters);
void motionCommandTask(void *pvParameters);
void wifiConnectionTask(void *pvParameters);
void estopResetTask(void *pvParameters);

bool setInternetControl(bool wifiControlEnable);
bool getInternetSettings();

bool stopSwitchTriggered = 0;

// create the OSSM hardware object
OSSM ossm;

///////////////////////////////////////////
////
////  VOID SETUP -- Here's where it's hiding
////
///////////////////////////////////////////

void setup()
{
    ossm.startLeds();
    Serial.begin(115200);
    LogDebug("\n Starting");
    pinMode(ENCODER_SWITCH, INPUT_PULLDOWN); // Rotary Encoder Pushbutton
    attachInterrupt(digitalPinToInterrupt(ENCODER_SWITCH), encoderPushButton, RISING);

    ossm.setup();
    ossm.findHome();

    ossm.setRunMode();

    // start the WiFi connection task so we can be doing something while homing!
    // xTaskCreatePinnedToCore(wifiConnectionTask,   /* Task function. */
    //                         "wifiConnectionTask", /* name of task. */
    //                         10000,                /* Stack size of task */
    //                         NULL,                 /* parameter of the task */
    //                         1,                    /* priority of the task */
    //                         &wifiTask,            /* Task handle to keep track of created task */
    //                         0);                   /* pin task to core 0 */
    // delay(100);

    // Kick off the http and motion tasks - they begin executing as soon as they
    // are created here! Do not change the priority of the task, or do so with
    // caution. RTOS runs first in first out, so if there are no delays in your
    // tasks they will prevent all other code from running on that core!
    xTaskCreatePinnedToCore(getUserInputTask,   /* Task function. */
                            "getUserInputTask", /* name of task. */
                            10000,              /* Stack size of task */
                            NULL,               /* parameter of the task */
                            1,                  /* priority of the task */
                            &getInputTask,      /* Task handle to keep track of created task */
                            0);                 /* pin task to core 0 */
    delay(100);
    xTaskCreatePinnedToCore(motionCommandTask,   /* Task function. */
                            "motionCommandTask", /* name of task. */
                            20000,               /* Stack size of task */
                            NULL,                /* parameter of the task */
                            1,                   /* priority of the task */
                            &motionTask,         /* Task handle to keep track of created task */
                            0);                  /* pin task to core 0 */

    delay(100);
    xTaskCreatePinnedToCore(estopResetTask,   /* Task function. */
                            "estopResetTask", /* name of task. */
                            10000,            /* Stack size of task */
                            NULL,             /* parameter of the task */
                            1,                /* priority of the task */
                            &estopTask,       /* Task handle to keep track of created task */
                            0);               /* pin task to core 0 */

    delay(100);
    ossm.g_ui.UpdateMessage("OSSM Ready to Play");
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
    switch(ossm.rightKnobMode) {
        case MODE_STROKE:
            ossm.g_ui.UpdateState("STROKE", static_cast<int>(ossm.speedPercentage), static_cast<int>(ossm.strokePercentage + 0.5f));
            break;
        case MODE_DEPTH:
            ossm.g_ui.UpdateState("DEPTH", static_cast<int>(ossm.speedPercentage), static_cast<int>(ossm.depthPercentage + 0.5f));
            break;
        case MODE_SENSATION:
            ossm.g_ui.UpdateState("SENSTN", static_cast<int>(ossm.speedPercentage), static_cast<int>(ossm.sensationPercentage + 0.5f));
            break;
        case MODE_PATTERN:
            ossm.g_ui.UpdateState("PATTRN", static_cast<int>(ossm.speedPercentage), ossm.strokePattern * 100 / (ossm.strokePatternCount - 1));
            break;
    }
    ossm.g_ui.UpdateScreen();

    // debug
    static bool is_connected = false;
    if (!is_connected && ossm.g_ui.DisplayIsConnected())
    {
        LogDebug("Display Connected");
        is_connected = true;
    }
    else if (is_connected && !ossm.g_ui.DisplayIsConnected())
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
            while ((ossm.getAnalogAveragePercent(SPEED_POT_PIN, 50) > 2))
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
    ossm.wifiConnectOrHotspotBlocking();
}

// Task to read settings from server - only need to check this when in WiFi
// control mode
void getUserInputTask(void *pvParameters)
{
    bool wifiControlEnable = false;
    for (;;) // tasks should loop forever and not return - or will throw error in
             // OS
    {
        // LogDebug("Speed: " + String(ossm.speedPercentage) + "\% Stroke: " + String(ossm.strokePercentage) +
        //          "\% Distance to target: " + String(ossm.stepper.getDistanceToTargetSigned()) + " steps?");

        ossm.updateAnalogInputs();

        ossm.speedPercentage > 1 ? ossm.stepper.releaseEmergencyStop() : ossm.stepper.emergencyStop();

        if (digitalRead(WIFI_CONTROL_TOGGLE_PIN) == HIGH) // TODO: check if wifi available and handle gracefully
        {
            if (wifiControlEnable == false)
            {
                // this is a transition to WiFi, we should tell the server it has
                // control
                wifiControlEnable = true;
                if (WiFi.status() != WL_CONNECTED)
                {
                    delay(5000);
                }
                setInternetControl(wifiControlEnable);
            }
            getInternetSettings(); // we load ossm.speedPercentage and ossm.strokePercentage in
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
            // ossm.speedPercentage = ossm.speedPercentage;
            // ossm.strokePercentage = getAnalogAverage(STROKE_POT_PIN, 50);
            // ossm.strokePercentage = ossm.getEncoderPercentage();
        }

        // We should scale these values with initialized settings not hard coded
        // values!
        if (ossm.speedPercentage > commandDeadzonePercentage)
        {
            ossm.stepper.setSpeedInMillimetersPerSecond(ossm.maxSpeedMmPerSecond * ossm.speedPercentage / 100.0);
            ossm.stepper.setAccelerationInMillimetersPerSecondPerSecond(
                ossm.maxSpeedMmPerSecond * ossm.speedPercentage * ossm.speedPercentage / ossm.accelerationScaling);
            // We do not set deceleration value here because setting a low decel when
            // going from high to low speed causes the motor to travel a long distance
            // before slowing. We should only change decel at rest
        }
        vTaskDelay(50); // let other code run!
    }
}

void motionCommandTask(void *pvParameters)
{
    for (;;) // tasks should loop forever and not return - or will throw error in
             // OS
    {
        switch (ossm.activeRunMode)
        {
            case ossm.simpleMode:
                ossm.runPenetrate();
                break;

            case ossm.strokeEngineMode:
                ossm.runStrokeEngine();
                break;
        }
    }
}

// float getAnalogVoltage(int pinNumber, int samples){

// }

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
    doc["stroke"] = ossm.strokePercentage;
    doc["speed"] = ossm.speedPercentage;
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
    ossm.strokePercentage = bubbleResponse["response"]["stroke"];
    ossm.speedPercentage = bubbleResponse["response"]["speed"];

    // debug info on the http payload
    LogDebug(payload);
    LogDebugFormatted("HTTP Response code: %d\n", httpResponseCode);

    return true;
}
// void setLedRainbow(CRGB leds[])
// {
//     // int power = 250;

//     for (int hueShift = 0; hueShift < 350; hueShift++)
//     {
//         int gHue = hueShift % 255;
//         fill_rainbow(leds, NUM_LEDS, gHue, 25);
//         FastLED.show();
//         delay(4);
//     }
// }
