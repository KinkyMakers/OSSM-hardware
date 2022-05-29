#ifndef UTILITIES_H
#define UTILITIES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP_FlexyStepper.h> // Current Motion Control
#include <Encoder.h>          // Used for the Remote Encoder Input
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "FastLED.h" // Used for the LED on the Reference Board (or any other pixel LEDS you may add)
#include "OSSM_Config.h"
#include "OSSM_PinDef.h"
#include "OssmUi.h" // Separate file that helps contain the OLED screen functions
#include "WiFi.h"
#include "WiFiManager.h"

#define BRIGHTNESS 170
#define LED_TYPE WS2811
#define COLOR_ORDER GRB
#define LED_PIN 25
#define NUM_LEDS 1
extern CRGB leds[NUM_LEDS];

class OSSM
{
   public:
    /**
     * @brief Construct a new ossm object
     */
    WiFiManager wm;
    ESP_FlexyStepper stepper;
    Encoder g_encoder;
    OssmUi g_ui;
    CRGB ossmleds[NUM_LEDS];

    float maxSpeedMmPerSecond = hardcode_maxSpeedMmPerSecond;
    float motorStepPerRevolution = hardcode_motorStepPerRevolution;
    float pulleyToothCount = hardcode_pulleyToothCount;
    float beltPitchMm = hardcode_beltPitchMm;
    float maxStrokeLengthMm = hardcode_maxStrokeLengthMm;
    float strokeZeroOffsetmm = hardcode_strokeZeroOffsetmm;
    float commandDeadzonePercentage = commandDeadzonePercentage;
    float accelerationScaling = hardcode_accelerationScaling;

    int hardwareVersion = 10; // V2.7 = integer value 27
    float currentSensorOffset = 0;
    float immediateCurrent = 0;
    float averageCurrent = 0;
    float numberStrokes = 0;
    float travelledDistanceMeters = 0;
    float lifeSecondsPowered = 0;
    float lifeSecondsPoweredAtStartup = 0;
    float lastLifeUpdateMillis = 0;
    float lastLifeWriteMillis = 0;
    char Id[20];

    float speedPercentage = 0;  // percentage 0-100
    float strokePercentage = 0; // percentage 0-100

    OSSM()
        : g_encoder(ENCODER_A, ENCODER_B),
          g_ui(REMOTE_ADDRESS, REMOTE_SDA, REMOTE_CLK) // this just creates the objects with parameters
    {
    }

    void setup();

    // WiFi helper functions
    void wifiAutoConnect();
    void wifiConnectOrHotspotBlocking();
    void updatePrompt();
    void updateFirmware();
    bool checkForUpdate();
    bool checkConnection();

    // hardware helper functions
    void initializeStepperParameters();
    void initializeInputs();
    bool findHome();
    float sensorlessHoming();
    void sensorHoming();
    int readEepromSettings();
    void writeEepromSettings();
    void writeEepromLifeStats();
    void updateLifeStats();

    // inputs
    void getAnalogInputs();
    float getCurrentReadingAmps(int samples);
    float getVoltageReading(int samples);

    float getAnalogAverage(int pinNumber, int samples);
    float getEncoderPercentage();
    bool waitForAnyButtonPress(float waitMilliseconds);
};

#endif
