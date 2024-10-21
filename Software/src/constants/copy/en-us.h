#ifndef OSSM_SOFTWARE_EN_US_H
#define OSSM_SOFTWARE_EN_US_H

#include "structs/LanguageStruct.h"

// English copy
static const LanguageStruct enUs = {
    .DeepThroatTrainerSync = "DeepThroat Sync",
    .Error = "Error",
    .GetHelp = "Get Help",
    .GetHelpLine1 = "On Discord,",
    .GetHelpLine2 = "or GitHub",
    .Homing = "Homing",
    .HomingTookTooLong =
        "Homing took too long. Please check your wiring and try again.",
    .Idle = "Initializing",
    .InDevelopment = "This feature is in development.",
    .MeasuringStroke = "Measuring Stroke",
    .NoInternalLoop = "No display handler implemented.",
    .Restart = "Restart",
    .Settings = "Settings",
    .SimplePenetration = "Simple Penetration",
    .Skip = "Click to exit",
    .Speed = "Speed",
    .SpeedWarning = "Decrease the speed to begin playing.",
    .StateNotImplemented = "State: %u not implemented.",
    .Stroke = "Stroke",
    .StrokeEngine = "Stroke Engine",
    .StrokeTooShort = "Stroke too short. Please check you drive belt.",
    .Update = "Update",
    .UpdateMessage = "Update is in progress. This may take up to 60s.",
    .WiFi = "Wi-Fi",
    .WiFiSetup = "Wi-Fi Setup",
    .WiFiSetupLine1 = "Connect to",
    .WiFiSetupLine2 = "'Ossm Setup'",
    .YouShouldNotBeHere = "You should not be here.",
    .StrokeEngineDescriptions = {
        "Acceleration, coasting, deceleration equally split; no sensation.",
        "Speed shifts with sensation; balances faster strokes.",
        "Sensation varies acceleration; from robotic to gradual.",
        "Full and half depth strokes alternate; sensation affects speed.",
        "Stroke depth increases per cycle; sensation sets count.",
        "Pauses between strokes; sensation adjusts length.",
        "Modifies length, maintains speed; sensation influences direction."
    },
    .StrokeEngineNames = {
        "Simple Stroke",
        "Teasing Pounding",
        "Robo Stroke",
        "Half'n'Half",
        "Deeper",
        "Stop'n'Go",
        "Insist"
    },
    .AdvancedConfiguration = "Advanced Settings",
    .AdvancedConfigurationSettingNames = {
        "Adv. Settings ReadMe",
        "Motor Direction",
        "Steps Per Revolution",
        "Pulley Teeth",
        "Max Speed",
        "Rail Length",
        "Rapid Homing",
        "Homing Sensitivity",
        "Reset to Defaults",
        "Apply Settings"
    },
    .AdvancedConfigurationSettingDescriptions = {
        "Long press to edit. \nShort press to set value. \nChanges revert unless applied.",
        "Homing dir toggle. \nFull depth should be penetration end away from OSSM Head.",
        "Set to match your motor register or dip switch setting.",
        "", // "Specify the number of teeth on your pulley.",
        "", // "Speed limit in millimeters per second.",
        "", // "The length of your rail in millimeters.",
        "", // "(Future)Feature toggle to run a faster homing procedure",
        "", // "This is a multiplier against idle current. \nHigher value will overcome more friction.",
        "", // "Restore settings to default and restart OSSM? \n Yes: Long press \n No: Short press",
        "Save changes and restart OSSM? \n Yes: Long press \n No: Short press"
    },
    // Reference board < 2.3 cannot handle this many strings being defined without the memory bootloop issue.
    // Refactor needed to reduce memory usage.

};

#endif  // OSSM_SOFTWARE_EN_US_H
