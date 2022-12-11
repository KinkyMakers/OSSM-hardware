#include "OSSMTCode.h"

#include <ESP_FlexyStepper.h> // Current Motion Control
#include <WiFiUdp.h>

#include "OSSM_Config.h"
#include "TCode.cpp"

#define FIRMWARE_ID "OSSMTCode.cpp" // Device and firmware version
#define TCODE_VER "TCode v0.3"      // Current version of TCode

#define TCODE_PORT 6969

OSSMTCode::OSSMTCode(ESP_FlexyStepper &stepper, float maxStrokeLengthMm) : stepper(stepper), tcode(FIRMWARE_ID, TCODE_VER)
{
    tcode.RegisterAxis("L0", "Up");
    this->maxStrokeLengthMm = maxStrokeLengthMm;

    // Acceleration calculation/motion planning is supposed to be handled by the TCode generator.
    stepper.setAccelerationInMillimetersPerSecondPerSecond(maxSpeedMmPerSecond * 100.0);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(maxSpeedMmPerSecond * 100.0);
    stepper.setSpeedInMillimetersPerSecond(maxSpeedMmPerSecond);
    wifiUdp.begin(TCODE_PORT);

    xTaskCreatePinnedToCore(OSSMTCode::inputTask,    /* Task function. */
                            "TCodeInputTask",        /* String with name of task (by default max 16 characters long) */
                            2000,                    /* Stack size in bytes. */
                            this,                    /* Parameter passed as input of the task */
                            1,                       /* Priority of the task, 1 seems to work just fine for us */
                            &this->xInputTaskHandle, /* Task handle. */
                            0);
};

#define TCODE_PRECISION 4
#define TCODE_KEEPOUT 5

void OSSMTCode::loop()
{
    while ((stepper.getDistanceToTargetSigned() != 0))
    {
        vTaskDelay(5);
        LogDebugFormatted("%d,%f,%d,%f,%f,wait\n", xLin, stepper.getCurrentPositionInMillimeters(),
                          stepper.getDistanceToTargetSigned(), stepper.getTargetPositionInMillimeters(),
                          stepper.getCurrentVelocityInMillimetersPerSecond());
    }

    xLin = tcode.AxisRead("L0");
    auto targetMm = map(xLin, 0, 9999, (0 + TCODE_KEEPOUT) * TCODE_PRECISION,
                        (maxStrokeLengthMm - TCODE_KEEPOUT) * TCODE_PRECISION) /
                    float(TCODE_PRECISION);
    stepper.setTargetPositionInMillimeters(targetMm);
    LogDebugFormatted("%d,%f,%d,%f,%f,command\n", xLin, stepper.getCurrentPositionInMillimeters(),
                      stepper.getDistanceToTargetSigned(), stepper.getTargetPositionInMillimeters(),
                      stepper.getCurrentVelocityInMillimetersPerSecond());
};

void OSSMTCode::inputTask(void *parameter)
{
    char packetBuffer[255];
    OSSMTCode *tRef = static_cast<OSSMTCode *>(parameter);
    for (;;)
    {
        int packetSize = tRef->wifiUdp.parsePacket();
        if (packetSize)
        {
            int len = tRef->wifiUdp.read(packetBuffer, 255);
            if (len > 0)
            {
                packetBuffer[len] = 0;
            }
            for (int i = 0; i++; i < len)
            {
                tRef->tcode.ByteInput(packetBuffer[i]);
            }
        }
        // Read serial and send to tcode class
        while (Serial.available() > 0)
        {
            // Send the serial bytes to the t-code object
            tRef->tcode.ByteInput(Serial.read());
        }
        vTaskDelay(100);
    }
}