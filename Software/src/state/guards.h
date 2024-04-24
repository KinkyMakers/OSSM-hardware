#ifndef SOFTWARE_GUARDS_H
#define SOFTWARE_GUARDS_H

#include "WiFi.h"
#include "constants/Menu.h"
#include "ossmi.h"
static auto isOnline = []() { return WiFiClass::status() == WL_CONNECTED; };

auto isOption = [](Menu option) {
    return [option](OSSMI &o) { return o.getMenuOption() == option; };
};

auto isStrokeTooShort = [](OSSMI &o) { return o.isStrokeTooShort(); };

#endif  // SOFTWARE_GUARDS_H
