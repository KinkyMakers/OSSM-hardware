#ifndef UTILITIES_H
#define UTILITIES_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ESP_FlexyStepper.h> // Current Motion Control
#include <Encoder.h>          // Used for the Remote Encoder Input
#include <HTTPClient.h>
#include <HTTPUpdate.h>

#include "OSSM_Config.h"
#include "OSSM_PinDef.h"
#include "OssmUi.h" // Separate file that helps contain the OLED screen functions
#include "WiFi.h"
#include "WiFiManager.h"

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

    // inputs
    void getAnalogInputs();
    float getCurrentReadingAmps(int samples);
    float getVoltageReading(int samples);

    float getAnalogAverage(int pinNumber, int samples);
    float getEncoderPercentage();
    bool waitForButtonPress(float waitMilliseconds);
};

#endif