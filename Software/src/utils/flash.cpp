#include "flash.h"

#include <Arduino.h>
#include <esp_flash.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>

#include "partition_layout.h"

namespace firmware {

std::uint32_t physicalFlashSizeBytes() {
    std::uint32_t size = 0;
    if (esp_flash_get_physical_size(nullptr, &size) == ESP_OK && size > 0) {
        return size;
    }
    return ESP.getFlashChipSize();
}

std::uint32_t otaSlotSizeBytes() {
    const esp_partition_t *partition =
        esp_ota_get_next_update_partition(nullptr);
    return partition == nullptr ? 0 : partition->size;
}

const char *currentPartitionLayout() {
    const esp_partition_t *app0 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
    const esp_partition_t *app1 = esp_partition_find_first(
        ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_1, nullptr);
    if (app0 == nullptr || app1 == nullptr) return "unknown";
    return detectOtaPartitionLayout(app0->address, app0->size,
                                     app1->address, app1->size);
}

}  // namespace firmware
