#ifndef OSSM_SOFTWARE_EN_US_H
#define OSSM_SOFTWARE_EN_US_H

#include "structs/LanguageStruct.h"
#include <EEPROM.h>

// English strings in PROGMEM
const char en_DeepThroatTrainerSync[] PROGMEM = "DeepThroat Sync";
const char en_Error[] PROGMEM = "Error";
const char en_GetHelp[] PROGMEM = "Get Help";
const char en_GetHelpLine1[] PROGMEM = "On Discord,";
const char en_GetHelpLine2[] PROGMEM = "or GitHub";
const char en_Homing[] PROGMEM = "Homing";
const char en_HomingTookTooLong[] PROGMEM = "Homing took too long. Please check your wiring and try again.";
const char en_Idle[] PROGMEM = "Initializing";
const char en_InDevelopment[] PROGMEM = "This feature is in development.";
const char en_MeasuringStroke[] PROGMEM = "Measuring Stroke";
const char en_NoInternalLoop[] PROGMEM = "No display handler implemented.";
const char en_Restart[] PROGMEM = "Restart";
const char en_Settings[] PROGMEM = "Settings";
const char en_SimplePenetration[] PROGMEM = "Simple Penetration";
const char en_Skip[] PROGMEM = "Click to exit";
const char en_Speed[] PROGMEM = "Speed";
const char en_SpeedWarning[] PROGMEM = "Decrease the speed to begin playing.";
const char en_StateNotImplemented[] PROGMEM = "State: %u not implemented.";
const char en_Stroke[] PROGMEM = "Stroke";
const char en_StrokeEngine[] PROGMEM = "Stroke Engine";
const char en_StrokeTooShort[] PROGMEM = "Stroke too short. Please check your drive belt.";
const char en_Pair[] PROGMEM = "Pair Device";
const char en_PairingInstructions[] PROGMEM = "Enter the following code on the dashboard";
const char en_Update[] PROGMEM = "Update";
const char en_UpdateMessage[] PROGMEM = "Update is in progress. This may take up to 60s.";
const char en_WiFi[] PROGMEM = "Wi-Fi";
const char en_WiFiSetup[] PROGMEM = "Wi-Fi Setup";
const char en_WiFiSetupLine1[] PROGMEM = "Connect to";
const char en_WiFiSetupLine2[] PROGMEM = "'Ossm Setup'";
const char en_YouShouldNotBeHere[] PROGMEM = "You should not be here.";

// Stroke Engine Descriptions
const char en_StrokeEngineDescriptions_0[] PROGMEM = "Acceleration, coasting, deceleration equally split; no sensation.";
const char en_StrokeEngineDescriptions_1[] PROGMEM = "Speed shifts with sensation; balances faster strokes.";
const char en_StrokeEngineDescriptions_2[] PROGMEM = "Sensation varies acceleration; from robotic to gradual.";
const char en_StrokeEngineDescriptions_3[] PROGMEM = "Full and half depth strokes alternate; sensation affects speed.";
const char en_StrokeEngineDescriptions_4[] PROGMEM = "Stroke depth increases per cycle; sensation sets count.";
const char en_StrokeEngineDescriptions_5[] PROGMEM = "Pauses between strokes; sensation adjusts length.";
const char en_StrokeEngineDescriptions_6[] PROGMEM = "Modifies length, maintains speed; sensation influences direction.";

// Stroke Engine Names
const char en_StrokeEngineNames_0[] PROGMEM = "Simple Stroke";
const char en_StrokeEngineNames_1[] PROGMEM = "Teasing Pounding";
const char en_StrokeEngineNames_2[] PROGMEM = "Robo Stroke";
const char en_StrokeEngineNames_3[] PROGMEM = "Half'n'Half";
const char en_StrokeEngineNames_4[] PROGMEM = "Deeper";
const char en_StrokeEngineNames_5[] PROGMEM = "Stop'n'Go";
const char en_StrokeEngineNames_6[] PROGMEM = "Insist";


// English copy
static const LanguageStruct enUs = {
        .DeepThroatTrainerSync = en_DeepThroatTrainerSync,
        .Error = en_Error,
        .GetHelp = en_GetHelp,
        .GetHelpLine1 = en_GetHelpLine1,
        .GetHelpLine2 = en_GetHelpLine2,
        .Homing = en_Homing,
        .HomingTookTooLong = en_HomingTookTooLong,
        .Idle = en_Idle,
        .InDevelopment = en_InDevelopment,
        .MeasuringStroke = en_MeasuringStroke,
        .NoInternalLoop = en_NoInternalLoop,
        .Restart = en_Restart,
        .Settings = en_Settings,
        .SimplePenetration = en_SimplePenetration,
        .Skip = en_Skip,
        .Speed = en_Speed,
        .SpeedWarning = en_SpeedWarning,
        .StateNotImplemented = en_StateNotImplemented,
        .Stroke = en_Stroke,
        .StrokeEngine = en_StrokeEngine,
        .StrokeTooShort = en_StrokeTooShort,
        .Pair = en_Pair,
        .PairingInstructions = en_PairingInstructions,
        .Update = en_Update,
        .UpdateMessage = en_UpdateMessage,
        .WiFi = en_WiFi,
        .WiFiSetup = en_WiFiSetup,
        .WiFiSetupLine1 = en_WiFiSetupLine1,
        .WiFiSetupLine2 = en_WiFiSetupLine2,
        .YouShouldNotBeHere = en_YouShouldNotBeHere,
        .StrokeEngineDescriptions = {
                en_StrokeEngineDescriptions_0,
                en_StrokeEngineDescriptions_1,
                en_StrokeEngineDescriptions_2,
                en_StrokeEngineDescriptions_3,
                en_StrokeEngineDescriptions_4,
                en_StrokeEngineDescriptions_5,
                en_StrokeEngineDescriptions_6
        },
        .StrokeEngineNames = {
                en_StrokeEngineNames_0,
                en_StrokeEngineNames_1,
                en_StrokeEngineNames_2,
                en_StrokeEngineNames_3,
                en_StrokeEngineNames_4,
                en_StrokeEngineNames_5,
                en_StrokeEngineNames_6
        }
};

#endif  // OSSM_SOFTWARE_EN_US_H
