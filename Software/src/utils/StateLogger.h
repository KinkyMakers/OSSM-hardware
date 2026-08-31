#ifndef OSSM_SOFTWARE_STATELOGGER_H
#define OSSM_SOFTWARE_STATELOGGER_H

#include <Arduino.h>

#include <cstring>

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
#if defined(CONFIG_ARDUHAL_ESP_LOG) && !defined(USE_ESP_IDF_LOG)
    // Arduino remaps ESP_LOG* to its own compile-time logging level.
    static constexpr bool debugEnabled =
        ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG;
#else
    static constexpr bool debugEnabled = LOG_LOCAL_LEVEL >= ESP_LOG_DEBUG;
#endif

    template <class SM, class TEvent>
    [[gnu::used]] void log_process_event(const TEvent&) {
        if constexpr (debugEnabled) {
            ESP_LOGV(STATE_MACHINE_TAG, "%s", sml::aux::get_type_name<SM>());
            const char* eventName = sml::aux::get_type_name<TEvent>();
            // Internal SML events are only visible at verbose level.
            if (std::strncmp(eventName, "boost::ext::sml",
                             sizeof("boost::ext::sml") - 1) == 0) {
                ESP_LOGV(STATE_MACHINE_TAG, "%s", eventName);
            } else {
                ESP_LOGD(STATE_MACHINE_TAG, "%s", eventName);
            }
        }
    }

    template <class SM, class TGuard, class TEvent>
    [[gnu::used]] void log_guard(const TGuard&, const TEvent&, bool result) {
        if constexpr (debugEnabled) {
            const char* resultString = result ? "[PASS]" : "[DO NOT PASS]";
            ESP_LOGV(STATE_MACHINE_TAG, "%s: %s", resultString,
                     sml::aux::get_type_name<SM>());
            ESP_LOGD(STATE_MACHINE_TAG, "%s: %s, %s", resultString,
                     sml::aux::get_type_name<TGuard>(),
                     sml::aux::get_type_name<TEvent>());
        }
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
