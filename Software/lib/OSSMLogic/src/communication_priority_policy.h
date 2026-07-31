#ifndef OSSM_COMMUNICATION_PRIORITY_POLICY_H
#define OSSM_COMMUNICATION_PRIORITY_POLICY_H

#include <cstdint>

namespace communication_priority_policy {

    enum class RadioPreference : uint8_t {
        Balanced,
        Bluetooth,
    };

    struct Policy {
        RadioPreference radioPreference;
        bool allowBackgroundNetworkWork;
    };

    constexpr uint32_t kBackgroundDeferralPollMilliseconds = 250;
    constexpr uint32_t kPairingStatusPollMilliseconds = 60000;
    constexpr int kMqttTaskPriority = 2;
    constexpr int kMqttTaskStackBytes = 6144;

    constexpr Policy forStreamingMode(bool streamingActive) {
        return {
            streamingActive ? RadioPreference::Bluetooth
                            : RadioPreference::Balanced,
            !streamingActive,
        };
    }

}  // namespace communication_priority_policy

#endif  // OSSM_COMMUNICATION_PRIORITY_POLICY_H
