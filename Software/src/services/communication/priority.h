#ifndef OSSM_COMMUNICATION_PRIORITY_H
#define OSSM_COMMUNICATION_PRIORITY_H

namespace communication_priority {

    // Streaming keeps Wi-Fi and MQTT alive, but gives Bluetooth first claim on
    // shared 2.4 GHz airtime and defers nonessential network transactions.
    void setStreamingActive(bool active);
    bool isStreamingActive();
    bool backgroundNetworkWorkAllowed();
    void refreshRadioPreference();

}  // namespace communication_priority

#endif  // OSSM_COMMUNICATION_PRIORITY_H
