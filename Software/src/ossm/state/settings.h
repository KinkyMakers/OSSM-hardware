#ifndef OSSM_STATE_SETTINGS_H
#define OSSM_STATE_SETTINGS_H

#include "structs/SettingPercents.h"

/**
 * Global settings - user-adjustable parameters
 * Exported from OSSM class static member
 */
extern SettingPercents settings;

// Helper function to get speed
inline int getSpeed() { return settings.speed; }

#endif  // OSSM_STATE_SETTINGS_H
