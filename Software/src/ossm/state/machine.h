#ifndef OSSM_STATE_MACHINE_H
#define OSSM_STATE_MACHINE_H

#include "boost/sml.hpp"

#include "actions.h"
#include "guards.h"
#include "../Events.h"
#include "../../utils/update.h"

namespace sml = boost::sml;

struct OSSMStateMachine {
    auto operator()() const {
        using namespace sml;
        using namespace actions;
        using namespace guards;

        return make_transition_table(
        // clang-format off
#ifdef AJ_DEVELOPMENT_HARDWARE
            *"idle"_s + done = "menu"_s,
#else
            *"idle"_s + done / drawHello = "homing"_s,
#endif

            "homing"_s / startHoming = "homing.forward"_s,
            "homing.forward"_s + error = "error"_s,
            "homing.forward"_s + done / startHoming = "homing.backward"_s,
            "homing.backward"_s + error = "error"_s,
            "homing.backward"_s + done[(isStrokeTooShort)] = "error"_s,
            "homing.backward"_s + done[isFirstHomed] / setHomed = "menu"_s,
            "homing.backward"_s + done[(isOption(Menu::SimplePenetration))] / setHomed = "simplePenetration"_s,
            "homing.backward"_s + done[(isOption(Menu::StrokeEngine))] / setHomed = "strokeEngine"_s,
            "homing.backward"_s + done[(isOption(Menu::Streaming))] / setHomed = "streaming"_s,

            "menu"_s / (drawMenu) = "menu.idle"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::SimplePenetration))] = "simplePenetration"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::StrokeEngine))] = "strokeEngine"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Streaming))] = "streaming"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Pairing)) && isOnline] = "pairing"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Pairing)) && !isOnline] = "pairing.wifi"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::UpdateOSSM))] = "update"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::WiFiSetup))] = "wifi"_s,
            "menu.idle"_s + buttonPress[isOption(Menu::Help)] = "help"_s,
            "menu.idle"_s + buttonPress[(isOption(Menu::Restart))] = "restart"_s,

            "simplePenetration"_s [isNotHomed] = "homing"_s,
            "simplePenetration"_s [isPreflightSafe] / (resetSettingsSimplePen, drawPlayControls, startSimplePenetration) = "simplePenetration.idle"_s,
            "simplePenetration"_s / drawPreflight = "simplePenetration.preflight"_s,
            "simplePenetration.preflight"_s + done / (resetSettingsSimplePen, drawPlayControls, startSimplePenetration) = "simplePenetration.idle"_s,
            "simplePenetration.preflight"_s + longPress = "menu"_s,
            "simplePenetration.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

            "strokeEngine"_s [isNotHomed] = "homing"_s,
            "strokeEngine"_s [isPreflightSafe] / (resetSettingsStrokeEngine, drawPlayControls, startStrokeEngine) = "strokeEngine.idle"_s,
            "strokeEngine"_s / drawPreflight = "strokeEngine.preflight"_s,
            "strokeEngine.preflight"_s + done / (resetSettingsStrokeEngine, drawPlayControls, startStrokeEngine) = "strokeEngine.idle"_s,
            "strokeEngine.preflight"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
            "strokeEngine.idle"_s + buttonPress / incrementControl = "strokeEngine.idle"_s,
            "strokeEngine.idle"_s + doublePress / drawPatternControls = "strokeEngine.pattern"_s,
            "strokeEngine.pattern"_s + buttonPress / drawPlayControls = "strokeEngine.idle"_s,
            "strokeEngine.pattern"_s + doublePress / drawPlayControls = "strokeEngine.idle"_s,
            "strokeEngine.pattern"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,
            "strokeEngine.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

            "streaming"_s [isNotHomed] = "homing"_s,
            "streaming"_s [isPreflightSafe] / ( drawPlayControls, startStreaming) = "streaming.idle"_s,
            "streaming"_s / drawPreflight = "streaming.preflight"_s,
            "streaming.preflight"_s + done / ( drawPlayControls, startStreaming) = "streaming.idle"_s,
            "streaming.preflight"_s + longPress = "menu"_s,
            "streaming.idle"_s + longPress / (emergencyStop, setNotHomed) = "menu"_s,

            "pairing"_s / checkPairing = "pairing.idle"_s,
            "pairing.idle"_s + done = "menu"_s,
            "pairing.idle"_s + buttonPress = "menu"_s,
            "pairing.idle"_s + longPress = "menu"_s,
            "pairing.idle"_s + error = "menu"_s,

            "pairing.wifi"_s / drawWiFi = "pairing.wifi.idle"_s,
            "pairing.wifi.idle"_s + done = "pairing"_s,
            "pairing.wifi.idle"_s + buttonPress = "menu"_s,
            "pairing.wifi.idle"_s + longPress = "menu"_s,

            "update"_s [isOnline] / drawUpdate = "update.checking"_s,
            "update"_s = "wifi"_s,
            "update.checking"_s [isUpdateAvailable] / (drawUpdating, updateOSSM) = "update.updating"_s,
            "update.checking"_s / drawNoUpdate = "update.idle"_s,
            "update.idle"_s + buttonPress = "menu"_s,
            "update.updating"_s  = X,

            "wifi"_s / drawWiFi = "wifi.idle"_s,
            "wifi.idle"_s + done / stopWifiPortal = "menu"_s,
            "wifi.idle"_s + buttonPress / stopWifiPortal = "menu"_s,
            "wifi.idle"_s + longPress / resetWiFi = "restart"_s,

            "help"_s / drawHelp = "help.idle"_s,
            "help.idle"_s + buttonPress = "menu"_s,

            "error"_s / drawError = "error.idle"_s,
            "error.idle"_s + buttonPress / drawHelp = "error.help"_s,
            "error.help"_s + buttonPress / restart = X,

            "restart"_s / restart = X);

        // clang-format on
    }
};

#endif  // OSSM_STATE_MACHINE_H
