#ifndef OSSM_SOFTWARE_MENU_H
#define OSSM_SOFTWARE_MENU_H

#include <Arduino.h>

#include "constants/UserConfig.h"

enum Menu {
    SimplePenetration,
    StrokeEngine,
    UpdateOSSM,
    WiFiSetup,
    Help,
    Restart,
    NUM_OPTIONS
};

static String menuStrings[Menu::NUM_OPTIONS] = {
    UserConfig::language.SimplePenetration,
    UserConfig::language.StrokeEngine,
    UserConfig::language.Update,
    UserConfig::language.WiFiSetup,
    UserConfig::language.GetHelp,
    UserConfig::language.Restart};

#endif  // OSSM_SOFTWARE_MENU_H
