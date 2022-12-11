#pragma once
#include <Arduino.h>
#include <ESP_FlexyStepper.h> // Current Motion Control
#include <WiFiUdp.h>

#include "OSSM_Config.h"
#include "TCode.cpp"

class OSSMTCode
{
    int xLin;
    ESP_FlexyStepper& stepper;
    TCode tcode;

    float maxSpeedMmPerSecond = hardcode_maxSpeedMmPerSecond;
    float maxStrokeLengthMm = hardcode_maxStrokeLengthMm;

    WiFiUDP wifiUdp;
    bool udpInitialized = false;

    TaskHandle_t xInputTaskHandle = NULL;

   public:
    OSSMTCode(ESP_FlexyStepper& stepper, float maxStrokeLengthMm);
    void loop();

   private:
    void static inputTask(void* parameter);
};
