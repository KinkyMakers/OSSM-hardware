/**
* @file actions.h
* @brief Defines actions for the OSSM state machine.
*
* This file contains the definitions of actions used in the OSSM state machine.
* Actions are operations that the state machine performs when certain transitions occur.
*
* The actions in this file are used in the transition table of the OSSMStateMachine class,
* which is defined in state.h. They are executed when certain events occur and the associated
* transitions are triggered.
*
* For example, the `drawHello` action might display a welcome message, the `drawError` action
* might display an error message, and the `drawGetHelp` action might display a help message.
*
* These actions are used in conjunction with the Boost SML library, which is a lightweight,
* header-only state machine library for C++.
*/

#ifndef SOFTWARE_ACTIONS_H
#define SOFTWARE_ACTIONS_H

#include "Utilities.h"
// Action definitions
auto initDevice = [](OSSM &o) {

};

auto drawHello = [](OSSM &o) {};
auto drawError = []() {};
auto drawGetHelp = []() {};
auto drawPreflight = []() {};
auto drawPlayControls = []() {};

#endif // SOFTWARE_ACTIONS_H
