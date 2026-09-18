#include "firmware_md5.h"

#include <esp_log.h>

namespace {
String cachedMd5;
}

void cacheSketchMd5() {
    if (cachedMd5.length() > 0) return;
    cachedMd5 = ESP.getSketchMD5();
    ESP_LOGW("MD5", "Sketch MD5 cached: %s", cachedMd5.c_str());
}

const String &sketchMd5() {
    if (cachedMd5.length() == 0) cacheSketchMd5();  // fallback, boot path missed
    return cachedMd5;
}
