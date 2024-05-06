#ifndef SOFTWARE_PREFERENCES_H
#define SOFTWARE_PREFERENCES_H

#pragma once

#include <Preferences.h>

static Preferences prefs;

static void initPreferences() { prefs.begin("OSSM", false); }

#endif  // SOFTWARE_PREFERENCES_H
