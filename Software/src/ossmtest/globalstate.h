#ifndef SOFTWARE_GLOBALSTATE_H
#define SOFTWARE_GLOBALSTATE_H
#include <boost/di.hpp>

#include "ossmi.h"
#include "ossmtest.h"
#include "state.h"
#include "utils/RecusiveMutex.h"
namespace di = boost::di;

auto injector = di::make_injector(di::bind<OSSMI>.to<OSSMTEST>().in(
    di::singleton)  // OSSM as singleton if shared state is desired
);


static auto sm = std::make_unique<sml::sm<SM, sml::thread_safe<ESP32RecursiveMutex>>>(injector.create<SM>());
#endif  // SOFTWARE_GLOBALSTATE_H
