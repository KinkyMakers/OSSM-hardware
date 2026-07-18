#include "update.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_system.h>

#include "FirmwareUpdateRuntime.h"
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

firmware::DeviceReport makeDeviceReport() {
    return {
        .deviceType = "ossm",
        .deviceId = std::string(WiFi.macAddress().c_str()),
        .reportedTrack = FIRMWARE_TRACK,
        .currentVersion = VERSION,
        .currentBuild = FIRMWARE_BUILD_SHA,
        .firmwareHash = "",
        .chip = std::string(ESP.getChipModel()),
        .hardwareRevision = "ossm-v1",
        .flashSizeBytes = ESP.getFlashChipSize(),
        .partitionLayout = "ossm-ota-v1",
    };
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

    ESP_LOGW(UPDATE_TAG,
             "Resolver assigned track=%s update=%s target=%s next=%s",
             decision.assignedTrack.c_str(),
             decision.updateAvailable ? "true" : "false",
             decision.targetVersion.c_str(), decision.nextHopVersion.c_str());
    if (!decision.updateAvailable) {
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

void ossmStartUpdate() {
    xTaskCreatePinnedToCore(updateTask, "updateTask",
                            20 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr,
                            0);
}
