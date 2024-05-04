#ifndef SOFTWARE_GUARD_H
#define SOFTWARE_GUARD_H

#include "WiFi.h"
#include "constants/Menu.h"
static auto isOnline = []() { return WiFiClass::status() == WL_CONNECTED; };

#endif  // SOFTWARE_GUARD_H
