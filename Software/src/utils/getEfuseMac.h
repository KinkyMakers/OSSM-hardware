#ifndef SOFTWARE_GETEFUSMAC_H
#define SOFTWARE_GETEFUSMAC_H

#include "Arduino.h"

static String getEfuseMacAddress() {
    uint64_t mac = ESP.getEfuseMac();
    uint8_t macArray[6];
    for (int i = 0; i < 6; i++) {
        macArray[i] = (mac >> (8 * i)) & 0xff;
    }
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X", macArray[5], macArray[4], macArray[3],
             macArray[2], macArray[1], macArray[0]);
    return String(macStr);
}

#endif //SOFTWARE_GETEFUSMAC_H
