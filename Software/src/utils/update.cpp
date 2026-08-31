#include "update.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_system.h>

#include "FirmwareUpdateRuntime.h"
#include "flash.h"
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
    report.hardwareRevision = "ossm-v1";
    report.flashSizeBytes = firmware::physicalFlashSizeBytes();
    report.psramSizeBytes = ESP.getPsramSize();
    report.otaSlotSizeBytes = firmware::otaSlotSizeBytes();
    report.partitionLayout = firmware::currentPartitionLayout();
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
