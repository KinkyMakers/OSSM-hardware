#ifndef SOFTWARE_TYPE_H
#define SOFTWARE_TYPE_H
#pragma once
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"
class OSSMStateMachine;
using OSSMState =
    sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
            sml::logger<StateLogger>>;

#endif  // SOFTWARE_TYPE_H
