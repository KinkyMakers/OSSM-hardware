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

struct UpdateRuntime {
    bool hasMqtt() const { return mqttClient != nullptr; }
    void stopMqtt() { esp_mqtt_client_stop(mqttClient); }
    void startMqtt() { esp_mqtt_client_start(mqttClient); }
    bool wifiConnected() const { return WiFi.status() == WL_CONNECTED; }
    firmware::DeviceReport makeReport() { return makeDeviceReport(); }
    bool postCheck(const firmware::DeviceReport &report,
                   firmware::Decision &decision, String &error) {
        return firmware::postCheck(RAD_SERVER, report, decision, error);
    }
    void observeCurrent(const firmware::DeviceReport &report,
                        const firmware::Decision &decision) {
        firmware::provenance::observeCurrent(report, decision);
    }
    void stageUpdate(const firmware::DeviceReport &report,
                     const firmware::Decision &decision) {
        firmware::provenance::stageUpdate(report, decision);
    }
    void logDecision(const firmware::Decision &decision) {
        ESP_LOGW(UPDATE_TAG,
                 "Resolver assigned track=%s shouldUpdate=%s target=%s next=%s reason=%s",
                 decision.assignedTrack.c_str(),
                 decision.shouldUpdate ? "true" : "false",
                 decision.targetVersion.c_str(), decision.nextHopVersion.c_str(),
                 decision.reason.c_str());
    }
    void logFailure(const String &reason) {
        ESP_LOGE(UPDATE_TAG, "Firmware update stopped: %s", reason.c_str());
    }
    void drawUpdating() { pages::drawUpdating(); }
    bool install(const firmware::Decision &decision, String &error) {
        return firmware::installApplicationAndFilesystem(decision, error);
    }
    void updateUnavailable() { stateMachine->process_event(UpdateUnavailable{}); }
    void deleteTask() { vTaskDelete(nullptr); }
    void restart() {
        ESP_LOGW(UPDATE_TAG, "Verified firmware installed; restarting");
        esp_restart();
    }
};

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

    UpdateRuntime runtime;
    firmware::runUpdateAttempt(runtime);
}

struct BootOps {
    using State = esp_ota_img_states_t;
    static constexpr State pendingState = ESP_OTA_IMG_PENDING_VERIFY;

    const esp_partition_t *runningPartition() {
        return esp_ota_get_running_partition();
    }
    bool stateFor(const esp_partition_t *partition, State &state) {
        return esp_ota_get_state_partition(partition, &state) == ESP_OK;
    }
    esp_err_t markValid() { return esp_ota_mark_app_valid_cancel_rollback(); }
    void logConfirmation(esp_err_t result) {
        if (result == ESP_OK) {
            ESP_LOGW(UPDATE_TAG, "Confirmed pending OTA application");
        } else {
            ESP_LOGE(UPDATE_TAG, "Failed to confirm pending OTA application: %s",
                     esp_err_to_name(result));
        }
    }
};

}  // namespace

void ossmConfirmRunningImage() {
    BootOps boot;
    firmware::confirmPendingImageWith(boot);
}

void ossmStartUpdate() {
    xTaskCreatePinnedToCore(updateTask, "updateTask",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}
