#ifndef OSSM_VERSION_STRUCT_H
#define OSSM_VERSION_STRUCT_H

#include "constants/Version.h"

// Numeric view of the firmware version baked into this build, used for the
// client-side update comparison in utils/update.h.
struct Version {
    int major = 0;
    int minor = 0;
    int patch = 0;
};

inline const Version currentVersion = {
    .major = MAJOR_VERSION,
    .minor = MINOR_VERSION,
    .patch = PATCH_VERSION,
};

#endif  // OSSM_VERSION_STRUCT_H
