#include "wm.h"

#include "WiFi.h"

WiFiManager wm;

void initWM() {
    WiFi.useStaticBuffers(true);
    WiFi.mode(WIFI_STA);

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
    WiFi.begin();
#endif

    // Wait for connection
    int retry = 0;
    const int max_retries = 5;  // Allow up to 5 seconds if 500ms per try
    while (WiFi.status() != WL_CONNECTED && retry < max_retries) {
        delay(500);
        retry++;
    }
}
