#ifndef SOFTWARE_GETEFUSMAC_H
#define SOFTWARE_GETEFUSMAC_H

#include "Arduino.h"

static String getMacAddress() {
#ifdef VERSIONDEV
    return String("AA:BB:CC:DD:EE:FF");
#endif
    return String(WiFi.macAddress());
}

#endif  // SOFTWARE_GETEFUSMAC_H
