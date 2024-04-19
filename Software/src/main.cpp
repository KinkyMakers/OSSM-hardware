#include <Arduino.h> // Basic Needs
#include <Wire.h>    // Used for i2c connections (Remote OLED Screen)

#include "OSSM_PinDef.h" // This is where you set pins specific for your board
#include "Utilities.h"   // Utility helper functions - wifi update and homing
#include "state/state.h"
#include "utils/StateLogger.h"

namespace sml = boost::sml;

// Homing
volatile bool g_has_not_homed = true;
bool REMOTE_ATTACHED = false;

// OSSM name setup
const char *ossmId = "OSSM1";
volatile int encoderButtonPresses = 0; // increment for each click
volatile long lastEncoderButtonPressMillis = 0;

IRAM_ATTR void encoderPushButton()
{
    // debounce check
    long currentTime = millis();
    if ((currentTime - lastEncoderButtonPressMillis) > 200)
    {
        // run interrupt if not run in last 50ms
        encoderButtonPresses++;
        lastEncoderButtonPressMillis = currentTime;
    }
}

// Create tasks for checking pot input or web server control, and task to handle
// planning the motion profile (this task is high level only and does not pulse
// the stepper!)
TaskHandle_t wifiTask = nullptr;
TaskHandle_t getInputTask = nullptr;
TaskHandle_t motionTask = nullptr;
TaskHandle_t estopTask = nullptr;
TaskHandle_t oledTask = nullptr;

// Declarations
void getUserInputTask(void *pvParameters);
void motionCommandTask(void *pvParameters);
void wifiConnectionTask(void *pvParameters);

// create the OSSM hardware object
OSSM ossm;
StateLogger ossmLogger;
OSSMState stateMachine{ossmLogger, ossm};

///////////////////////////////////////////
////
////  VOID SETUP -- Here's where it's hiding
////
///////////////////////////////////////////

void setup()
{
    Serial.begin(115200);

    ossm.startLeds();
    ESP_LOGD("UTILS", "Starting");
    pinMode(ENCODER_SWITCH, INPUT_PULLDOWN); // Rotary Encoder Pushbutton
    attachInterrupt(digitalPinToInterrupt(ENCODER_SWITCH), encoderPushButton, RISING);

    ossm.setup();

    // start the WiFi connection task so we can be doing something while homing!
    xTaskCreatePinnedToCore(wifiConnectionTask,   /* Task function. */
                            "wifiConnectionTask", /* name of task. */
                            10000,                /* Stack size of task */
                            nullptr,              /* parameter of the task */
                            1,                    /* priority of the task */
                            &wifiTask,            /* Task handle to keep track of created task */
                            0);                   /* pin task to core 0 */
    delay(100);

    ossm.findHome();

    //ossm.setRunMode();

    // Kick off the http and motion tasks - they begin executing as soon as they
    // are created here! Do not change the priority of the task, or do so with
    // caution. RTOS runs first in first out, so if there are no delays in your
    // tasks they will prevent all other code from running on that core!
    xTaskCreatePinnedToCore(getUserInputTask,   /* Task function. */
                            "getUserInputTask", /* name of task. */
                            10000,              /* Stack size of task */
                            nullptr,            /* parameter of the task */
                            1,                  /* priority of the task */
                            &getInputTask,      /* Task handle to keep track of created task */
                            0);                 /* pin task to core 0 */
    delay(100);
    xTaskCreatePinnedToCore(motionCommandTask,   /* Task function. */
                            "motionCommandTask", /* name of task. */
                            20000,               /* Stack size of task */
                            nullptr,             /* parameter of the task */
                            1,                   /* priority of the task */
                            &motionTask,         /* Task handle to keep track of created task */
                            0);                  /* pin task to core 0 */

    delay(100);
    OssmUi::UpdateMessage("OSSM Ready to Play");
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
    switch (ossm.rightKnobMode)
    {
        case MODE_STROKE:
            OssmUi::UpdateState("STROKE", static_cast<int>(ossm.speedPercentage),
                                static_cast<int>(ossm.strokePercentage + 0.5f));
            break;
        case MODE_DEPTH:
            OssmUi::UpdateState("DEPTH", static_cast<int>(ossm.speedPercentage),
                                static_cast<int>(ossm.depthPercentage + 0.5f));
            break;
        case MODE_SENSATION:
            OssmUi::UpdateState("SENSTN", static_cast<int>(ossm.speedPercentage),
                                static_cast<int>(ossm.sensationPercentage + 0.5f));
            break;
        case MODE_PATTERN:
            OssmUi::UpdateState(ossm.strokerPatternName, static_cast<int>(ossm.speedPercentage),
                                ossm.strokePattern * 100 / (ossm.strokePatternCount - 1));
            break;
    }
}

///////////////////////////////////////////
////
////
////  freeRTOS multitasking
////
////
///////////////////////////////////////////

void wifiConnectionTask(void *pvParameters)
{
    ossm.wifiConnectOrHotspotNonBlocking();
}

// Task to read settings from server - only need to check this when in WiFi
// control mode
void getUserInputTask(void *pvParameters)
{
    float targetSpeedMmPerSecond = 0;
    float targetAccelerationMm = 0;
    float previousSpeedPercentage = 0;
    for (;;) // tasks should loop forever and not return - or will throw error in
             // OS
    {
        ossm.updateAnalogInputs();
        ossm.handleStopCondition();

        if (digitalRead(WIFI_CONTROL_TOGGLE_PIN) == HIGH) // TODO: check if wifi available and handle gracefully
        {
            ossm.enableWifiControl();
        }
        else
        {
            if (ossm.wifiControlActive == true)
            {
                // this is a transition to local control, we should tell the server it cannot control

                ossm.setInternetControl(false);
            }
        }

        // We should scale these values with initialized settings not hard coded
        // values!
        if (ossm.speedPercentage > ossm.commandDeadzonePercentage)
        {
            targetSpeedMmPerSecond = ossm.maxSpeedMmPerSecond * ossm.speedPercentage / 100.0;
            if ((ossm.speedPercentage - previousSpeedPercentage) > 30)
            {
                targetSpeedMmPerSecond =
                    0.5 * (ossm.speedPercentage + previousSpeedPercentage) * ossm.maxSpeedMmPerSecond / 100.0;
            }
            targetAccelerationMm =
                ossm.maxSpeedMmPerSecond * ossm.speedPercentage * ossm.speedPercentage / ossm.accelerationScaling;

            ossm.stepper.setAccelerationInMillimetersPerSecondPerSecond(targetAccelerationMm);
            if (targetSpeedMmPerSecond > ossm.stepper.getCurrentVelocityInMillimetersPerSecond())
            { // we are speeding up, we need to increase deccel rate!
                ossm.stepper.setDecelerationInMillimetersPerSecondPerSecond(targetAccelerationMm);
            }

            ossm.stepper.setSpeedInMillimetersPerSecond(targetSpeedMmPerSecond);

            // If target speed is lower than current, we do not update deccel as setting a low decel when going from
            // high to low speed causes the motor to travel a long distance before slowing.
        }
        previousSpeedPercentage = ossm.speedPercentage;
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
