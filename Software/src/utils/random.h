#ifndef UUID_HPP
#define UUID_HPP

#include <Arduino.h>

inline String uuid() {
    // Generate a random UUID v4 (xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx)
    uint8_t uuidBytes[16];
    for (int i = 0; i < 16; ++i) {
        uuidBytes[i] = (uint8_t)esp_random();
    }
    // Set the version (4) and variant (10xx)
    uuidBytes[6] = (uuidBytes[6] & 0x0F) | 0x40;  // Version 4
    uuidBytes[8] = (uuidBytes[8] & 0x3F) | 0x80;  // Variant 10xx

    char uuidStr[37];
    snprintf(
        uuidStr, sizeof(uuidStr),
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
        uuidBytes[0], uuidBytes[1], uuidBytes[2], uuidBytes[3], uuidBytes[4],
        uuidBytes[5], uuidBytes[6], uuidBytes[7], uuidBytes[8], uuidBytes[9],
        uuidBytes[10], uuidBytes[11], uuidBytes[12], uuidBytes[13],
        uuidBytes[14], uuidBytes[15]);
    return String(uuidStr);
}

#endif  // UUID_HPP
