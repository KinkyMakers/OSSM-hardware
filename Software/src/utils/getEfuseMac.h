#ifndef SOFTWARE_GETEFUSMAC_H
#define SOFTWARE_GETEFUSMAC_H

#include "Arduino.h"

static String getMacAddress() {
#ifdef VERSIONDEV
    return String("AA:BB:CC:DD:EE:FF");
#endif
    uint64_t mac = ESP.getEfuseMac();
    String macStr = "";
    for (int i = 5; i >= 0; i--) {
        macStr += String((mac >> (8 * i)) & 0xFF, HEX);
        if (i > 0) {
            macStr += ":";
        }
    }
    macStr.toUpperCase();
    return macStr;
}

#endif  // SOFTWARE_GETEFUSMAC_H
