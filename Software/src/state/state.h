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

/**
 * @brief Defines the state machine for the OSSM project.
 *
 * The OSSMStateMachine class uses the Boost SML library to define a state
 * machine for managing state transitions. It is designed to be thread-safe, so
 * events can be processed in any thread or task.
 *
 * Here are the basics of boost SML:
 *
 * 1. Each row in the table is a transition.
 * 2. Each transition has a source state, an event, a guard, an action, and a
 * target state. ex: "idle"_s + done / drawHello = "homing"_s,
 *    - In this example, the source state is "idle", the event is "done", the
 * guard is "none", the action is "drawHello", and the target state is "homing".
 * 3. The source state is the state that the machine must be in for the
 * transition to be valid.
 * 4. (optional) The event is the event that triggers the transition.
 * 5. (optional) The guard is a function that returns true or false. If it
 * returns true, then the transition is valid.
 * 6. (optional) The action is a function that is called when the transition is
 * triggered, it can't block the main thread and cannot return a value.
 * 7. The target state is the state that the machine will be in after the
 * transition is complete.
 *
 * For more information, see the Boost SML documentation:
 * https://boost-ext.github.io/sml/index.html
 */
class OSSMStateMachine {
  public:
    auto operator()() const {
        return make_transition_table(
            // clang-format off
            *"idle"_s + on_entry<_> / initDevice,
            "idle"_s + done / drawHello = "homing"_s,

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

using OSSMState =
    sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
            sml::logger<StateLogger>>;

#endif  // SOFTWARE_STATE_H
