#ifndef OSSM_SOFTWARE_LANGUAGESTRUCT_H
#define OSSM_SOFTWARE_LANGUAGESTRUCT_H

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
    const char* Stroke;
    const char* StrokeEngine;
    const char* StrokeTooShort;
    const char* Pair;
    const char* PairingInstructions;
    const char* PairingTookTooLong;
    const char* Update;
    const char* UpdateMessage;
    const char* WiFi;
    const char* WiFiSetup;
    const char* WiFiSetupLine1;
    const char* WiFiSetupLine2;
    const char* YouShouldNotBeHere;
    const char* StrokeEngineDescriptions[7];
    const char* StrokeEngineNames[7];
};

//static const char* getString(const char* progmemString) {
//    static char buffer[100]; // Adjust size as needed
//    strcpy_P(buffer, progmemString);
//    return buffer;
//}

#endif  // OSSM_SOFTWARE_LANGUAGESTRUCT_H
