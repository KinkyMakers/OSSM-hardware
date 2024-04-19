/**
 * @file events.h
 * @brief Defines events for the OSSM state machine.
 *
 * This file contains the definitions of events used in the OSSM state machine.
 * Events are instances of certain occurrences or changes in the system that can
 * trigger state transitions in the state machine.
 *
 * The events in this file are used in the transition table of the
 * OSSMStateMachine class, which is defined in state.h. They are used to trigger
 * transitions between different states of the state machine.
 *
 * For example, the `ButtonPress` event represents a button press action, and
 * the `LongPress` event represents a long press action. The `Done` event
 * signifies the completion of a task, and the `Error` event signifies an error
 * occurrence.
 *
 * These events are used in conjunction with the Boost SML library, which is a
 * lightweight, header-only state machine library for C++.
 */

#ifndef SOFTWARE_EVENTS_H
#define SOFTWARE_EVENTS_H

#include "boost/sml.hpp"
namespace sml = boost::sml;
using namespace sml;

struct ButtonPress {};
struct LongPress {};
struct Done {};
struct Error {};

// Definitions to make the table easier to read.
static auto buttonPress = sml::event<ButtonPress>;
static auto longPress = sml::event<LongPress>;
static auto done = sml::event<Done>;
static auto error = sml::event<Error>;

#endif  // SOFTWARE_EVENTS_H
