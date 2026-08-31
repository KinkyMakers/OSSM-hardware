#include "update.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_flash.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#include "FirmwareUpdateRuntime.h"
#include <OssmHardwareVariant.h>
#include "constants/LogTags.h"
#include "constants/Version.h"
#include "ossm/Events.h"
#include "ossm/pages/update.h"
#include "ossm/state/state.h"
#include "services/communication/mqtt.h"

#ifndef FIRMWARE_BUILD_SHA
#define FIRMWARE_BUILD_SHA "unknown"
#endif

#ifndef FIRMWARE_TRACK
#define FIRMWARE_TRACK "main"
#endif

namespace {

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
    if (app0->address == 0x10000 && app0->size == 0x780000 &&
        app1->address == 0x790000 && app1->size == 0x780000) {
        return "ossm-ota-16mb-v1";
    }
    if (app0->address == 0x10000 && app0->size == 0x1E0000 &&
        app1->address == 0x1F0000 && app1->size == 0x1E0000) {
        return "ossm-ota-4mb-v1";
    }
    return "unknown";
}

std::string runningFirmwareHash() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    unsigned char digest[32] = {};
    if (running == nullptr || esp_partition_get_sha256(running, digest) != ESP_OK) {
        return "";
    }
    return firmware::sha256Hex(digest);
}

firmware::DeviceReport makeDeviceReport() {
    firmware::DeviceReport report;
    report.deviceType = "ossm";
    report.deviceId = std::string(WiFi.macAddress().c_str());
    report.reportedTrack = FIRMWARE_TRACK;
    report.currentVersion = VERSION;
    report.currentBuild = FIRMWARE_BUILD_SHA;
    report.firmwareHash = runningFirmwareHash();
    report.chip = std::string(ESP.getChipModel());
    report.chipRevision = ESP.getChipRevision();
    report.chipCores = ESP.getChipCores();
    report.hardwareRevision =
        std::string("ossm-") + ossm_hardware::HARDWARE_VARIANT;
    report.flashSizeBytes = physicalFlashSizeBytes();
    report.psramSizeBytes = ESP.getPsramSize();
    report.otaSlotSizeBytes = otaSlotSizeBytes();
    report.partitionLayout = currentPartitionLayout();
    firmware::provenance::reconcile(report);
    return report;
}

void finishWithoutUpdate(bool mqttStopped, const String &reason) {
    if (!reason.isEmpty()) {
        ESP_LOGE(UPDATE_TAG, "Firmware update stopped: %s", reason.c_str());
    }
    if (mqttStopped) {
        esp_mqtt_client_start(mqttClient);
    }
    stateMachine->process_event(UpdateUnavailable{});
    vTaskDelete(nullptr);
}

// The complete HTTPS check and install remains isolated from the button task.
// MQTT is paused so its TLS session cannot compete with the update client for
// heap; motor control is never invoked by this task.
void updateTask(void *pvParameters) {
    ESP_LOGW(UPDATE_TAG,
             "Update task started: %s %s (%s), heap=%lu, largest=%lu",
             VERSION, FIRMWARE_BUILD_SHA, FIRMWARE_TRACK,
             static_cast<unsigned long>(esp_get_free_heap_size()),
             static_cast<unsigned long>(
                 heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)));

    bool mqttStopped = false;
    if (mqttClient != nullptr) {
        esp_mqtt_client_stop(mqttClient);
        mqttStopped = true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        finishWithoutUpdate(mqttStopped, "Wi-Fi is disconnected");
        return;
    }

    firmware::Decision decision;
    String error;
    const firmware::DeviceReport report = makeDeviceReport();
    if (!firmware::postCheck(RAD_SERVER, report, decision, error)) {
        finishWithoutUpdate(mqttStopped, error);
        return;
    }
    firmware::provenance::observeCurrent(report, decision);
    firmware::provenance::stageUpdate(report, decision);

    ESP_LOGW(UPDATE_TAG,
             "Resolver assigned track=%s shouldUpdate=%s target=%s next=%s reason=%s",
             decision.assignedTrack.c_str(),
             decision.shouldUpdate ? "true" : "false",
             decision.targetVersion.c_str(), decision.nextHopVersion.c_str(),
             decision.reason.c_str());
    if (!decision.shouldUpdate) {
        finishWithoutUpdate(mqttStopped, "");
        return;
    }

    pages::drawUpdating();
    if (!firmware::installApplicationAndFilesystem(decision, error)) {
        finishWithoutUpdate(mqttStopped, error);
        return;
    }

    ESP_LOGW(UPDATE_TAG, "Verified firmware installed; restarting");
    esp_restart();
}

}  // namespace

void ossmConfirmRunningImage() {
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t state;
    if (running == nullptr ||
        esp_ota_get_state_partition(running, &state) != ESP_OK ||
        state != ESP_OTA_IMG_PENDING_VERIFY) {
        return;
    }

    const esp_err_t result = esp_ota_mark_app_valid_cancel_rollback();
    if (result == ESP_OK) {
        ESP_LOGW(UPDATE_TAG, "Confirmed pending OTA application");
    } else {
        ESP_LOGE(UPDATE_TAG, "Failed to confirm pending OTA application: %s",
                 esp_err_to_name(result));
    }
}

void ossmStartUpdate() {
    xTaskCreatePinnedToCore(updateTask, "updateTask",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}
