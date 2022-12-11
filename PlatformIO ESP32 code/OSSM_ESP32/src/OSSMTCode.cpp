#include "OSSMTCode.h"

#include <ESP_FlexyStepper.h> // Current Motion Control
#include <WiFiUdp.h>

#include <functional>

#include "OSSM_Config.h"
#include "TCode.cpp"

#define FIRMWARE_ID "OSSMTCode.cpp" // Device and firmware version
#define TCODE_VER "TCode v0.3"      // Current version of TCode

// todo make this a class member
TCode tcode(FIRMWARE_ID, TCODE_VER);

#define TCODE_PORT 6969

void OSSMTCode::setup(ESP_FlexyStepper stepper, float maxStrokeLengthMm)
{
    tcode.RegisterAxis("L0", "Up");
    this->stepper = stepper;
    this->maxStrokeLengthMm = maxStrokeLengthMm;

    stepper.setAccelerationInMillimetersPerSecondPerSecond(hardcode_maxSpeedMmPerSecond * 100.0);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(hardcode_maxSpeedMmPerSecond * 100.0);
    stepper.setSpeedInMillimetersPerSecond(hardcode_maxSpeedMmPerSecond);

    wifiUdp.begin(TCODE_PORT);

    xTaskCreatePinnedToCore(OSSMTCode::inputTask,    /* Task function. */
                            "TCodeInputTask",        /* String with name of task (by default max 16 characters long) */
                            512,                     /* Stack size in bytes. */
                            this,                    /* Parameter passed as input of the task */
                            1,                       /* Priority of the task, 1 seems to work just fine for us */
                            &this->xInputTaskHandle, /* Task handle. */
                            0);
};

void OSSMTCode::loop()
{
    xLin = tcode.AxisRead("L0");
    auto targetMm = map(xLin, 0, 9999, 0 + 5, maxStrokeLengthMm - 5);
    stepper.setTargetPositionInMillimeters(targetMm);
    vTaskDelay(10);
};

void OSSMTCode::inputTask(void *parameter)
{
    char packetBuffer[255];
    OSSMTCode *tCodeRef = static_cast<OSSMTCode *>(parameter);
    for (;;)
    {
        int packetSize = tCodeRef->wifiUdp.parsePacket();
        if (packetSize)
        {
            int len = tCodeRef->wifiUdp.read(packetBuffer, 255);
            if (len > 0)
            {
                packetBuffer[len] = 0;
            }
            for (int i = 0; i++; i < len)
            {
                tcode.ByteInput(packetBuffer[i]);
            }
        }
        // Read serial and send to tcode class
        while (Serial.available() > 0)
        {
            // Send the serial bytes to the t-code object
            tcode.ByteInput(Serial.read());
        }
        vTaskDelay(10);
    }
}