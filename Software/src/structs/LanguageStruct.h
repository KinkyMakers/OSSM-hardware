#ifndef OSSM_SOFTWARE_LANGUAGESTRUCT_H
#define OSSM_SOFTWARE_LANGUAGESTRUCT_H

#include <Arduino.h>

struct LanguageStruct {
    const char* DeepThroatTrainerSync;
    const char* Error;
    const char* GetHelp;
    const char* GetHelpLine1;
    const char* GetHelpLine2;
    const char* Homing;
    const char* HomingTookTooLong;
    const char* Idle;
    const char* InDevelopment;
    const char* MeasuringStroke;
    const char* NoInternalLoop;
    const char* Restart;
    const char* Settings;
    const char* SimplePenetration;
    const char* Skip;
    const char* Speed;
    const char* SpeedWarning;
    const char* StateNotImplemented;
    const char* Streaming;
    const char* Stroke;
    const char* StrokeEngine;
    const char* StrokeTooShort;
    const char* Update;
    const char* UpdateMessage;
    const char* WiFi;
    const char* WiFiSetup;
    const char* WiFiSetupLine1;
    const char* WiFiSetupLine2;
    const char* YouShouldNotBeHere;
    const char* StrokeEngineDescriptions[9];
    const char* StrokeEngineNames[9];
};

#endif  // OSSM_SOFTWARE_LANGUAGESTRUCT_H
