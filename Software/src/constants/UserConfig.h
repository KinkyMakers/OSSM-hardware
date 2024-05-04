#ifndef OSSM_SOFTWARE_USERCONFIG_H
#define OSSM_SOFTWARE_USERCONFIG_H

#include "constants/copy/en-us.h"
#include "constants/copy/fr.h"

namespace UserConfig {
    // TODO: restore user overrides.

    // TODO: Create a simple way in the UI to change the language.
    //  Minimally, all we need to do is change the copy struct to the following:
    //      const LanguageStruct copy = fr;
    //  or any other language.
    static LanguageStruct language = enUs;
    static bool displayMetric = true;
}
#endif  // OSSM_SOFTWARE_USERCONFIG_H
