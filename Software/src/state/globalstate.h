#ifndef SOFTWARE_GLOBALSTATE_H
#define SOFTWARE_GLOBALSTATE_H
#include <boost/di.hpp>

//#include "ossm/OSSM.h"
#include "ossmi.h"
#include "state.h"
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"

#pragma once
namespace di = boost::di;

class OSSM;

auto injector = di::make_injector(di::bind<OSSMI>.to<OSSM>().in(
    di::singleton)  // OSSM as singleton if shared state is desired
);

// create OSSM instance with injector

static OSSMI &ossm = injector.create<OSSMI &>();
static StateLogger stateLogger;

static auto stateMachine =
    sml::sm<SM, sml::thread_safe<ESP32RecursiveMutex>,
            sml::logger<StateLogger>>(injector.create<SM>(), stateLogger, ossm);
#endif  // SOFTWARE_GLOBALSTATE_H
