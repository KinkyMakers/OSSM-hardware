#include "memory.h"

#include <esp_littlefs.h>
#include <stdio.h>

JsonDocument registry;

bool initLittleFS() {
    ESP_LOGI("MEMORY", "Initializing LittleFS...");

    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/littlefs",
        .partition_label = "spiffs",
        .format_if_mount_failed = true,
        .dont_mount = false,
    };

    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE("MEMORY", "Failed to initialize LittleFS (%s)",
                 esp_err_to_name(ret));
        return false;
    }

    size_t total = 0, used = 0;
    esp_littlefs_info(conf.partition_label, &total, &used);
    ESP_LOGI("MEMORY", "LittleFS partition: total=%d, used=%d", total, used);

    FILE *f = fopen("/littlefs/registry.json", "r");
    if (f) {
        ESP_LOGI("MEMORY", "registry.json exists");
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);

        char *buf = (char *)malloc(size + 1);
        if (buf) {
            fread(buf, 1, size, f);
            buf[size] = '\0';

            DeserializationError error = deserializeJson(registry, buf);
            if (error) {
                ESP_LOGE("MEMORY", "Failed to parse registry.json: %s",
                         error.c_str());
            } else {
                ESP_LOGI("MEMORY", "registry.json parsed successfully");
            }
            free(buf);
        }
        fclose(f);
    }

    return true;
}
