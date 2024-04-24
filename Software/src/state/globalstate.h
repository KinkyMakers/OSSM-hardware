#ifndef SOFTWARE_GLOBALSTATE_H
#define SOFTWARE_GLOBALSTATE_H
#include <boost/di.hpp>

#include "ossm/OSSM.h"
#include "ossmi.h"
#include "state.h"
#include "utils/RecusiveMutex.h"
#include "utils/StateLogger.h"

namespace di = boost::di;

auto injector = di::make_injector(di::bind<OSSMI>.to<OSSM>().in(
    di::singleton)  // OSSM as singleton if shared state is desired
);

static StateLogger stateLogger;

static auto stateMachine =
    std::make_unique<sml::sm<SM, sml::thread_safe<ESP32RecursiveMutex>,
                             sml::logger<StateLogger>>>(injector.create<SM>(),
                                                        stateLogger);
#endif  // SOFTWARE_GLOBALSTATE_H
