#ifndef OSSM_STATE_GUARDS_H
#define OSSM_STATE_GUARDS_H

#include <WiFi.h>

#include "../../constants/Menu.h"

// Forward declarations for guard implementations (defined in guards.cpp)
bool ossmIsStrokeTooShort();
bool ossmIsNotHomed();
bool ossmIsPreflightSafe();
Menu ossmGetMenuOption();

namespace guards {

    // Guard for checking if stroke is too short
    constexpr auto isStrokeTooShort = []() {
#ifdef AJ_DEVELOPMENT_HARDWARE
        return false;
#else
        return ossmIsStrokeTooShort();
#endif
    };

    // Guard for checking menu option - returns a lambda that checks if current option matches
    constexpr auto isOption = [](Menu option) {
        return [option]() { return ossmGetMenuOption() == option; };
    };

    // Guard for checking if preflight is safe
    constexpr auto isPreflightSafe = []() {
#ifdef AJ_DEVELOPMENT_HARDWARE
        return true;
#else
        return ossmIsPreflightSafe();
#endif
    };

    // Guard for checking if this is the first homing
    constexpr auto isFirstHomed = []() {
        static bool firstHomed = true;
        if (firstHomed) {
            firstHomed = false;
            return true;
        }
        return false;
    };

    // Guard for checking if not homed
    constexpr auto isNotHomed = []() { return ossmIsNotHomed(); };

    // Guard for checking if online
    constexpr auto isOnline = []() { return WiFiClass::status() == WL_CONNECTED; };

}  // namespace guards

#endif  // OSSM_STATE_GUARDS_H
