#ifndef OSSM_SOFTWARE_LANGUAGESTRUCT_H
#define OSSM_SOFTWARE_LANGUAGESTRUCT_H

struct LanguageStruct {
    const String& DeepThroatTrainerSync;
    const String& Error;
    const String& GetHelp;
    const String& GetHelpLine1;
    const String& GetHelpLine2;
    const String& Homing;
    const String& HomingTookTooLong;
    const String& Idle;
    const String& InDevelopment;
    const String& MeasuringStroke;
    const String& NoInternalLoop;
    const String& Restart;
    const String& Settings;
    const String& SimplePenetration;
    const String& Skip;
    const String& Speed;
    const String& SpeedWarning;
    const String& StateNotImplemented;
    const String& Stroke;
    const String& StrokeEngine;
    const String& StrokeTooShort;
    const String& Update;
    const String& UpdateMessage;
    const String& WiFi;
    const String& WiFiSetup;
    const String& WiFiSetupLine1;
    const String& WiFiSetupLine2;
    const String& YouShouldNotBeHere;
    const String (&StrokeEngineDescriptions)[7];
    const String (&StrokeEngineNames)[7];
    const String& AdvancedConfiguration;
    const String (&AdvancedConfigurationSettingNames)[10];
    const String (&AdvancedConfigurationSettingDescriptions)[10];
};

#endif  // OSSM_SOFTWARE_LANGUAGESTRUCT_H
