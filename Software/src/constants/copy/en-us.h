#ifndef OSSM_SOFTWARE_EN_US_H
#define OSSM_SOFTWARE_EN_US_H

#include "structs/LanguageStruct.h"

// English copy - All strings stored in PROGMEM
static const char enUs_DeepThroatTrainerSync[] PROGMEM = "DeepThroat Sync";
static const char enUs_Error[] PROGMEM = "Error";
static const char enUs_GetHelp[] PROGMEM = "Get Help";
static const char enUs_GetHelpLine1[] PROGMEM = "On Discord,";
static const char enUs_GetHelpLine2[] PROGMEM = "or GitHub";
static const char enUs_Homing[] PROGMEM = "Homing";
static const char enUs_HomingTookTooLong[] PROGMEM =
    "Homing took too long. Please check your wiring and try again.";
static const char enUs_Idle[] PROGMEM = "Initializing";
static const char enUs_InDevelopment[] PROGMEM =
    "This feature is in development.";
static const char enUs_MeasuringStroke[] PROGMEM = "Measuring Stroke";
static const char enUs_NoInternalLoop[] PROGMEM =
    "No display handler implemented.";
static const char enUs_Restart[] PROGMEM = "Restart";
static const char enUs_Settings[] PROGMEM = "Settings";
static const char enUs_SimplePenetration[] PROGMEM = "Simple Penetration";
static const char enUs_Skip[] PROGMEM = "Click to exit";
static const char enUs_Speed[] PROGMEM = "Speed";
static const char enUs_SpeedWarning[] PROGMEM =
    "Decrease the speed to begin playing.";
static const char enUs_StateNotImplemented[] PROGMEM =
    "State: %u not implemented.";
static const char enUs_Streaming[] PROGMEM = "Streaming (beta)";
static const char enUs_Stroke[] PROGMEM = "Stroke";
static const char enUs_StrokeEngine[] PROGMEM = "Stroke Engine";
static const char enUs_StrokeTooShort[] PROGMEM =
    "Stroke too short. Please check you drive belt.";
static const char enUs_Pairing[] PROGMEM = "Pairing";
static const char enUs_Update[] PROGMEM = "Update";
static const char enUs_UpdateMessage[] PROGMEM =
    "Update is in progress. This may take up to 60s.";
static const char enUs_WiFi[] PROGMEM = "Wi-Fi";
static const char enUs_WiFiSetup[] PROGMEM = "Wi-Fi Setup";
static const char enUs_WiFiSetupLine1[] PROGMEM = "Connect to";
static const char enUs_WiFiSetupLine2[] PROGMEM = "'Ossm Setup'";
static const char enUs_YouShouldNotBeHere[] PROGMEM = "You should not be here.";

static const char enUs_StrokeEngineDescriptions_0[] PROGMEM =
    "Acceleration, coasting, deceleration equally split; Sensation adds randomness in stroke";
static const char enUs_StrokeEngineDescriptions_1[] PROGMEM =
    "Speed shifts with sensation; balances faster strokes.";
static const char enUs_StrokeEngineDescriptions_2[] PROGMEM =
    "Sensation varies acceleration; from robotic to gradual.";
static const char enUs_StrokeEngineDescriptions_3[] PROGMEM =
    "Full and half depth strokes alternate; sensation affects speed.";
static const char enUs_StrokeEngineDescriptions_4[] PROGMEM =
    "Stroke depth increases per cycle; sensation sets count.";
static const char enUs_StrokeEngineDescriptions_5[] PROGMEM =
    "Pauses between strokes; sensation adjusts length.";
static const char enUs_StrokeEngineDescriptions_6[] PROGMEM =
    "Stroke length decreaes per cycle to set depth; sensation sets count.";
static const char enUs_StrokeEngineDescriptions_7[] PROGMEM =
    "Strokes are made of substrokes controlled by sensation.";
static const char enUs_StrokeEngineDescriptions_8[] PROGMEM =
    "Random Stroke. Sensation controls maximum randomness from current location.";
static const char enUs_StrokeEngineDescriptions_9[] PROGMEM =
    "Moves to a point between depth and stroke based on sensation.";

static const char enUs_StrokeEngineNames_0[] PROGMEM = "Simple Stroke";
static const char enUs_StrokeEngineNames_1[] PROGMEM = "Teasing Pounding";
static const char enUs_StrokeEngineNames_2[] PROGMEM = "Robo Stroke";
static const char enUs_StrokeEngineNames_3[] PROGMEM = "Half'n'Half";
static const char enUs_StrokeEngineNames_4[] PROGMEM = "Deeper";
static const char enUs_StrokeEngineNames_5[] PROGMEM = "Stop'n'Go";
static const char enUs_StrokeEngineNames_6[] PROGMEM = "Insist";
static const char enUs_StrokeEngineNames_7[] PROGMEM = "Progressive Stroke";
static const char enUs_StrokeEngineNames_8[] PROGMEM = "Random Stroke";
static const char enUs_StrokeEngineNames_9[] PROGMEM = "Go to Point";

static const LanguageStruct enUs = {
    .DeepThroatTrainerSync = enUs_DeepThroatTrainerSync,
    .Error = enUs_Error,
    .GetHelp = enUs_GetHelp,
    .GetHelpLine1 = enUs_GetHelpLine1,
    .GetHelpLine2 = enUs_GetHelpLine2,
    .Homing = enUs_Homing,
    .HomingTookTooLong = enUs_HomingTookTooLong,
    .Idle = enUs_Idle,
    .InDevelopment = enUs_InDevelopment,
    .MeasuringStroke = enUs_MeasuringStroke,
    .NoInternalLoop = enUs_NoInternalLoop,
    .Restart = enUs_Restart,
    .Settings = enUs_Settings,
    .SimplePenetration = enUs_SimplePenetration,
    .Skip = enUs_Skip,
    .Speed = enUs_Speed,
    .SpeedWarning = enUs_SpeedWarning,
    .StateNotImplemented = enUs_StateNotImplemented,
    .Streaming = enUs_Streaming,
    .Stroke = enUs_Stroke,
    .StrokeEngine = enUs_StrokeEngine,
    .StrokeTooShort = enUs_StrokeTooShort,
    .Pairing = enUs_Pairing,
    .Update = enUs_Update,
    .UpdateMessage = enUs_UpdateMessage,
    .WiFi = enUs_WiFi,
    .WiFiSetup = enUs_WiFiSetup,
    .WiFiSetupLine1 = enUs_WiFiSetupLine1,
    .WiFiSetupLine2 = enUs_WiFiSetupLine2,
    .YouShouldNotBeHere = enUs_YouShouldNotBeHere,
    .StrokeEngineDescriptions = {enUs_StrokeEngineDescriptions_0,
                                 enUs_StrokeEngineDescriptions_1,
                                 enUs_StrokeEngineDescriptions_2,
                                 enUs_StrokeEngineDescriptions_3,
                                 enUs_StrokeEngineDescriptions_4,
                                 enUs_StrokeEngineDescriptions_5,
                                 enUs_StrokeEngineDescriptions_6,
                                 enUs_StrokeEngineDescriptions_7,
                                 enUs_StrokeEngineDescriptions_8,
                                 enUs_StrokeEngineDescriptions_9
    },
    .StrokeEngineNames = {enUs_StrokeEngineNames_0,
                          enUs_StrokeEngineNames_1,
                          enUs_StrokeEngineNames_2,
                          enUs_StrokeEngineNames_3,
                          enUs_StrokeEngineNames_4,
                          enUs_StrokeEngineNames_5,
                          enUs_StrokeEngineNames_6,
                          enUs_StrokeEngineNames_7,
                          enUs_StrokeEngineNames_8,
                          enUs_StrokeEngineNames_9
    }
};

#endif  // OSSM_SOFTWARE_EN_US_H
