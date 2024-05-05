#ifndef SOFTWARE_UNIQUEIDENTITY_H
#define SOFTWARE_UNIQUEIDENTITY_H

#include <Arduino.h>

#include "UUID.h"
#include "services/preferences.h"

static UUID uuid;

// String get Unique Identity
String getUniqueIdentity() {
    // Try to get ID from perfs

    String uniqueIdentity = perfs.getString("uuid", "");

    // if the ID is greater than zero, return it
    if (uniqueIdentity.length() > 0) {
        return uniqueIdentity;
    }

    ESP_LOGD("UniqueIdentity", "Generating new UUID");

    uint32_t seed1 = random(999999999);
    uint32_t seed2 = random(999999999);
    uuid.seed(seed1, seed2);

    uuid.setVariant4Mode();

    // uuid.generate();
    //  to string
    uuid.generate();

    uniqueIdentity = uuid.toCharArray();

    // Save the ID to perfs
    perfs.putString("uuid", uniqueIdentity);

    return uniqueIdentity;
}

#endif  // SOFTWARE_UNIQUEIDENTITY_H
