#ifndef OSSM_SOFTWARE_STATELOGGER_H
#define OSSM_SOFTWARE_STATELOGGER_H

#include <Arduino.h>

#include <cassert>

#include "boost/sml.hpp"

namespace sml = boost::sml;
using namespace sml;

static const char* TAG = "StateLogger";

/**
 * OSSM State Logger
 *
 * This class is used to log state machine events.
 *
 * It is used by the OSSM class in OSSM.h.
 * By default this project will only show "LOG_DEBUG" and above.
 * You can change this in the platformio.ini file, or by added one of the
 * following build flags:
 *
 * -D LOG_LEVEL=LOG_LEVEL_NONE
 * -D LOG_LEVEL=LOG_LEVEL_ERROR
 * -D LOG_LEVEL=LOG_LEVEL_WARN
 * -D LOG_LEVEL=LOG_LEVEL_INFO
 * -D LOG_LEVEL=LOG_LEVEL_DEBUG
 * -D LOG_LEVEL=LOG_LEVEL_TRACE
 *
 */
struct StateLogger
{
    template <class SM, class TEvent>
    [[gnu::used]] void log_process_event(const TEvent&)
    {
        ESP_LOGV(TAG, "%s", sml::aux::get_type_name<SM>());
        String eventName = String(sml::aux::get_type_name<TEvent>());
        // if the event name starts with " boost::ext::sml" then only TRACE it
        // to reduce verbosity
        if (eventName.startsWith("boost::ext::sml"))
        {
            ESP_LOGV(TAG, "%s", eventName.c_str());
        }
        else
        {
            ESP_LOGD(TAG, "%s", eventName.c_str());
        }
    }

    template <class SM, class TGuard, class TEvent>
    [[gnu::used]] void log_guard(const TGuard&, const TEvent&, bool result)
    {
        String resultString = result ? "[PASS]" : "[DO NOT PASS]";
        ESP_LOGV(TAG, "%s: %s", resultString, sml::aux::get_type_name<SM>());
        ESP_LOGD(TAG, "%s: %s, %s", resultString, sml::aux::get_type_name<TGuard>(),
                 sml::aux::get_type_name<TEvent>());
    }

    template <class SM, class TAction, class TEvent>
    [[gnu::used]] void log_action(const TAction&, const TEvent&)
    {
        ESP_LOGV(TAG, "%s", sml::aux::get_type_name<SM>());

        // These are trace messages because lambda functions are very verbose.
        ESP_LOGV(TAG, "%s, %s", sml::aux::get_type_name<TAction>(), sml::aux::get_type_name<TEvent>());
    }

    template <class SM, class TSrcState, class TDstState>
    [[gnu::used]] void log_state_change(const TSrcState& src, const TDstState& dst)
    {
        ESP_LOGV(TAG, "%s", sml::aux::get_type_name<SM>());
        ESP_LOGD(TAG, "%s -> %s", src.c_str(), dst.c_str());
    }
};
#endif // OSSM_SOFTWARE_STATELOGGER_H
