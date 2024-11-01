#ifndef OSSM_SOFTWARE_EN_US_H
#define OSSM_SOFTWARE_EN_US_H

#include "structs/LanguageStruct.h"

static const String enUs_DeepThroatTrainerSync PROGMEM = "DeepThroat Sync";
static const String enUs_Error PROGMEM = "Error";
static const String enUs_GetHelp PROGMEM = "Get Help";
static const String enUs_GetHelpLine1 PROGMEM = "On Discord,";
static const String enUs_GetHelpLine2 PROGMEM = "or GitHub";
static const String enUs_Homing PROGMEM = "Homing";
static const String enUs_HomingTookTooLong PROGMEM =
    "Homing took too long. Please check your wiring and try again.";
static const String enUs_Idle PROGMEM = "Initializing";
static const String enUs_InDevelopment PROGMEM =
    "This feature is in development.";
static const String enUs_MeasuringStroke PROGMEM = "Measuring Stroke";
static const String enUs_NoInternalLoop PROGMEM =
    "No display handler implemented.";
static const String enUs_Restart PROGMEM = "Restart";
static const String enUs_Settings PROGMEM = "Settings";
static const String enUs_SimplePenetration PROGMEM = "Simple Penetration";
static const String enUs_Skip PROGMEM = "Click to exit";
static const String enUs_Speed PROGMEM = "Speed";
static const String enUs_SpeedWarning PROGMEM =
    "Decrease the speed to begin playing.";
static const String enUs_StateNotImplemented PROGMEM =
    "State: %u not implemented.";
static const String enUs_Stroke PROGMEM = "Stroke";
static const String enUs_StrokeEngine PROGMEM = "Stroke Engine";
static const String enUs_StrokeTooShort PROGMEM =
    "Stroke too short. Please check you drive belt.";
static const String enUs_Update PROGMEM = "Update";
static const String enUs_UpdateMessage PROGMEM =
    "Update is in progress. This may take up to 60s.";
static const String enUs_WiFi PROGMEM = "Wi-Fi";
static const String enUs_WiFiSetup PROGMEM = "Wi-Fi Setup";
static const String enUs_WiFiSetupLine1 PROGMEM = "Connect to";
static const String enUs_WiFiSetupLine2 PROGMEM = "'Ossm Setup'";
static const String enUs_YouShouldNotBeHere PROGMEM = "You should not be here.";
static const String enUs_AdvancedConfiguration PROGMEM = "Advanced Settings";

// stroke engine progmem
static const String enUs_StrokeEngineDescriptions[] PROGMEM = {
    "Acceleration, coasting, deceleration equally split; no sensation.",
    "Speed shifts with sensation; balances faster strokes.",
    "Sensation varies acceleration; from robotic to gradual.",
    "Full and half depth strokes alternate; sensation affects speed.",
    "Stroke depth increases per cycle; sensation sets count.",
    "Pauses between strokes; sensation adjusts length.",
    "Modifies length, maintains speed; sensation influences direction."};

static const String enUs_StrokeEngineNames[] PROGMEM = {
    "Simple Stroke", "Teasing Pounding", "Robo Stroke", "Half'n'Half",
    "Deeper",        "Stop'n'Go",        "Insist"};

static const String enUs_AdvancedConfigurationSettingNames[] PROGMEM = {
    "Adv. Settings ReadMe", "Motor Direction",    "Steps Per Revolution",
    "Pulley Teeth",         "Max Speed",          "Rail Length",
    "Rapid Homing",         "Homing Sensitivity", "Reset to Defaults",
    "Apply Settings"};

static const String enUs_AdvancedConfigurationSettingDescriptions[] PROGMEM = {
    "Long press to edit. \nShort press to set value. \nChanges revert "
    "unless applied.",
    "Homing dir toggle. \nFull depth should be penetration end away from "
    "OSSM Head.",
    "Set to match your motor register or dip switch setting.",
    "Specify the number of teeth on your pulley.",
    "Speed limit in millimeters per second.",
    "The length of your rail in millimeters.",
    "(Future)Feature toggle to run a faster homing procedure",
    "This is a multiplier against idle current. \nHigher value will "
    "overcome more friction.",
    "Restore settings to default and restart OSSM? \n Yes: Long press \n "
    "No: Short press",
    "Save changes and restart OSSM? \n Yes: Long press \n No: Short press"};

// English copy
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
    .Stroke = enUs_Stroke,
    .StrokeEngine = enUs_StrokeEngine,
    .StrokeTooShort = enUs_StrokeTooShort,
    .Update = enUs_Update,
    .UpdateMessage = enUs_UpdateMessage,
    .WiFi = enUs_WiFi,
    .WiFiSetup = enUs_WiFiSetup,
    .WiFiSetupLine1 = enUs_WiFiSetupLine1,
    .WiFiSetupLine2 = enUs_WiFiSetupLine2,
    .YouShouldNotBeHere = enUs_YouShouldNotBeHere,
    .StrokeEngineDescriptions = enUs_StrokeEngineDescriptions,
    .StrokeEngineNames = enUs_StrokeEngineNames,
    .AdvancedConfiguration = enUs_AdvancedConfiguration,
    .AdvancedConfigurationSettingNames = enUs_AdvancedConfigurationSettingNames,
    .AdvancedConfigurationSettingDescriptions =
        enUs_AdvancedConfigurationSettingDescriptions};

#endif  // OSSM_SOFTWARE_EN_US_H
