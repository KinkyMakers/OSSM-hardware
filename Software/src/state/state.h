#ifndef SOFTWARE_STATE_H
#define SOFTWARE_STATE_H

#include "Utilities.h"
#include "actions.h"
#include "boost/sml.hpp"
#include "constants/Menu.h"
#include "events.h"
#include "guards.h"
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"
namespace sml = boost::sml;
using namespace sml;

class OSSMStateMachine
{
   public:
    auto operator()() const
    {
        return make_transition_table(
            // clang-format off
            *"idle"_s + done / drawHello = "homing"_s,

            "homing"_s + done = "menu"_s,
            "homing"_s + error = "error"_s,

            "menu"_s + buttonPress[(isOption(Menu::SimplePenetration))] = "simplePenetration"_s,
            "menu"_s + buttonPress[isOption(Menu::StrokeEngine)] = "strokeEngine"_s,
            "menu"_s + buttonPress[isOption(Menu::Help)] = "help"_s,
            "menu"_s + buttonPress[isOption(Menu::Restart)] = "restart"_s,

            "simplePenetration"_s + on_entry<_> / drawPreflight,
            "simplePenetration"_s + done[isPreflightSafe] = "simplePenetration.play"_s,
            "simplePenetration"_s + longPress = "menu"_s,
            "simplePenetration"_s + error = "error"_s,
            "simplePenetration.play"_s + on_entry<_> / drawPlayControls,
            "simplePenetration.play"_s + error = "error"_s,
            "simplePenetration.play"_s + longPress = "menu"_s,

            "strokeEngine"_s + on_entry<_> / drawPreflight,
            "strokeEngine"_s + done[isPreflightSafe] = "strokeEngine.play"_s,
            "strokeEngine"_s + longPress = "menu"_s,
            "strokeEngine"_s + error = "error"_s,
            "strokeEngine.play"_s + on_entry<_> / drawPlayControls,
            "strokeEngine.play"_s + error = "error"_s,
            "strokeEngine.play"_s + longPress = "menu"_s,

            "help"_s + on_entry<_> / drawGetHelp,
            "help"_s + buttonPress = "menu"_s,
            "help"_s + longPress = "menu"_s,

            "error"_s + on_entry<_> / drawError,
            "error"_s + longPress / drawGetHelp = X,
            "error"_s + buttonPress / drawGetHelp = X
        );
        // clang-format on
    }
};

using OSSMState = sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>, sml::logger<StateLogger>>;

#endif // SOFTWARE_STATE_H
