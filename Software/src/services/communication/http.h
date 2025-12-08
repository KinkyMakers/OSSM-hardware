#ifndef OSSM_HTTP_H
#define OSSM_HTTP_H

#include <Arduino.h>

namespace HttpService {

    // Initialize HTTP service (call once at startup)
    void init();

    // Queue a POST request (non-blocking, returns immediately)
    void queuePost(const String& endpoint, const String& jsonPayload);

    // Get device MAC address (useful for device identification)
    String getMacAddress();

}

#endif
