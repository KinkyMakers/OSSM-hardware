#ifndef OSSM_SOFTWARE_STATELOGGER_H
#define OSSM_SOFTWARE_STATELOGGER_H

#include <Arduino.h>

#include <cassert>

#include "boost/sml.hpp"
#include "constants/LogTags.h"

namespace sml = boost::sml;
using namespace sml;

/**
 * @brief Logs state machine events for the OSSM class.
 *
 * The StateLogger class is responsible for logging the events of the state
 * machine used in the OSSM class. The logging level can be adjusted according
 * to the project's needs. By default, only messages with a level of "LOG_DEBUG"
 * or above are shown. This can be modified in the platformio.ini file or by
 * adding one of the following build flags:
 */
struct StateLogger {
    template <class SM, class TEvent>
    [[gnu::used]] void log_process_event(const TEvent&) {
        ESP_LOGV(STATE_MACHINE_TAG, "%s", sml::aux::get_type_name<SM>());
        String eventName = String(sml::aux::get_type_name<TEvent>());
        // if the event name starts with " boost::ext::sml" then only TRACE it
        // to reduce verbosity
        if (eventName.startsWith("boost::ext::sml")) {
            ESP_LOGV(STATE_MACHINE_TAG, "%s", eventName.c_str());
        } else {
            ESP_LOGD(STATE_MACHINE_TAG, "%s", eventName.c_str());
        }
    }

    template <class SM, class TGuard, class TEvent>
    [[gnu::used]] void log_guard(const TGuard&, const TEvent&, bool result) {
        String resultString = result ? "[PASS]" : "[DO NOT PASS]";
        ESP_LOGV(STATE_MACHINE_TAG, "%s: %s", resultString,
                 sml::aux::get_type_name<SM>());
        ESP_LOGD(STATE_MACHINE_TAG, "%s: %s, %s", resultString,
                 sml::aux::get_type_name<TGuard>(),
                 sml::aux::get_type_name<TEvent>());
    }

    template <class SM, class TAction, class TEvent>
    [[gnu::used]] void log_action(const TAction&, const TEvent&) {
        ESP_LOGV(STATE_MACHINE_TAG, "%s", sml::aux::get_type_name<SM>());
    }

    template <class SM, class TSrcState, class TDstState>
    [[gnu::used]] void log_state_change(const TSrcState& src,
                                        const TDstState& dst) {
        ESP_LOGV(STATE_MACHINE_TAG, "%s", sml::aux::get_type_name<SM>());
        ESP_LOGD(STATE_MACHINE_TAG, "%s -> %s", src.c_str(), dst.c_str());
    }
};
#endif  // OSSM_SOFTWARE_STATELOGGER_H
