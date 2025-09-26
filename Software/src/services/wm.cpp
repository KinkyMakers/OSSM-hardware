#include "wm.h"

#include "WiFi.h"

WiFiManager wm;

void initWM() {
    WiFi.useStaticBuffers(true);

#if defined(WIFI_SSID) && defined(WIFI_PASSWORD)
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
#else
    WiFi.begin();
#endif
}
