#include "priority.h"

#include <Arduino.h>
#include <esp_coexist.h>
#include <esp_log.h>

#include <atomic>

#include "communication_priority_policy.h"

namespace communication_priority {
    namespace {

        std::atomic<bool> streamingActive{false};

        void applyRadioPreference(bool active) {
            const auto policy =
                communication_priority_policy::forStreamingMode(active);
            const esp_coex_prefer_t preference =
                policy.radioPreference ==
                        communication_priority_policy::RadioPreference::Bluetooth
                    ? ESP_COEX_PREFER_BT
                    : ESP_COEX_PREFER_BALANCE;
            const esp_err_t result = esp_coex_preference_set(preference);
            if (result != ESP_OK) {
                ESP_LOGW("COMM_PRIORITY",
                         "COMM_PRIORITY event=radio_preference_failed "
                         "streaming=%d error=%d",
                         active, static_cast<int>(result));
                return;
            }
            ESP_LOGI("COMM_PRIORITY",
                     "COMM_PRIORITY event=radio_preference streaming=%d "
                     "preference=%s background_network=%d free_heap=%u",
                     active, active ? "bluetooth" : "balanced",
                     policy.allowBackgroundNetworkWork,
                     static_cast<unsigned>(ESP.getFreeHeap()));
        }

    }  // namespace

    void setStreamingActive(bool active) {
        const bool previous =
            streamingActive.exchange(active, std::memory_order_acq_rel);
        if (previous != active) applyRadioPreference(active);
    }

    bool isStreamingActive() {
        return streamingActive.load(std::memory_order_acquire);
    }

    bool backgroundNetworkWorkAllowed() {
        return communication_priority_policy::forStreamingMode(
                   isStreamingActive())
            .allowBackgroundNetworkWork;
    }

    void refreshRadioPreference() {
        applyRadioPreference(isStreamingActive());
    }

}  // namespace communication_priority
