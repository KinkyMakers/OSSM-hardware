/**
 * @file guards.h
 * @brief Defines guard conditions for the OSSM state machine.
 *
 * This file contains the definitions of guard conditions used in the OSSM state
 * machine. Guard conditions are predicates that determine whether a transition
 * should be taken in response to an event. They are defined as functions or
 * functors that take the necessary parameters and return a boolean value.
 *
 * The guards in this file are used in the transition table of the
 * OSSMStateMachine class, which is defined in state.h. They are used to add
 * conditional logic to the state machine's transitions.
 *
 * For example, the `isPreflightSafe` guard checks if the speed percentage is
 * zero, and the `isOption` guard checks if a certain menu option is selected.
 *
 * These guards are used in conjunction with the Boost SML library, which is a
 * lightweight, header-only state machine library for C++.
 */

#ifndef SOFTWARE_GUARDS_H
#define SOFTWARE_GUARDS_H

#pragma once

class OSSM;  // Forward declaration of class OSSM

// Guard definitions
auto isPreflightSafe = [](OSSM &o) {
    // TODO: Implement this
    return o.speedPercentage == 0;
};
auto isOption = [](Menu option) {
    // TODO: Implement this
    return [option]() { return option && true; };
};

#endif  // SOFTWARE_GUARDS_H
