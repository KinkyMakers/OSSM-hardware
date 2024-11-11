#ifndef OSSM_SOFTWARE_MENU_H
#define OSSM_SOFTWARE_MENU_H

#include <Arduino.h>

#include "constants/UserConfig.h"

enum Menu {
    SimplePenetration,
    StrokeEngine,
    Settings,
    NUM_OPTIONS
};

enum MenuSettings {
    UpdateOSSM,
    WiFiSetup,
    Help,
    Restart,
    SETTINGS_NUM_OPTIONS
};

static String menuStrings[Menu::NUM_OPTIONS] = {
    UserConfig::language.SimplePenetration,
    UserConfig::language.StrokeEngine,
    UserConfig::language.Settings};

static String menuSettingsStrings[MenuSettings::SETTINGS_NUM_OPTIONS] = {
    UserConfig::language.Update,
    UserConfig::language.WiFiSetup,
    UserConfig::language.GetHelp,
    UserConfig::language.Restart};

#endif  // OSSM_SOFTWARE_MENU_H
