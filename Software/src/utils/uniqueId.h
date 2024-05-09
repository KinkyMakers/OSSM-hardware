#ifndef SOFTWARE_UNIQUEID_H
#define SOFTWARE_UNIQUEID_H

#include "UUID.h"
#include "services/preferences.h"

static UUID uuid;

static String generateId() {
    // Generate two random seeds using esp_random() function
    uint32_t seed1 = esp_random();
    uint32_t seed2 = esp_random();

    // Seed the UUID generator with the generated seeds and set it to variant 4
    // mode
    uuid.seed(seed1, seed2);
    uuid.setVariant4Mode();

    // Generate a new UUID
    uuid.generate();

    // Convert the generated UUID to a character array and assign it to the id
    // variable
    String id = uuid.toCharArray();

    // Return the generated UUID
    return id;
}

/**
 * Retrieves a unique identifier (UUID) from device preferences. If a valid UUID
 * is found in the preferences, it returns that UUID. Otherwise, it generates a
 * new UUID using random seeds and saves it to the preferences for future use.
 *
 * @return String - The unique identifier (UUID) string.
 */
static String getId() {
    // Retrieve the UUID string from device preferences
    String id = prefs.getString("uuid", "");

    // If the retrieved UUID string has a valid length (36 characters), return
    // it
    if (id.length() == 36) {
        return id;
    }

    id = generateId();

    // Save the generated UUID to device preferences for future use
    prefs.putString("uuid", id);

    // Return the generated UUID
    return id;
}

#endif  // SOFTWARE_UNIQUEID_H
