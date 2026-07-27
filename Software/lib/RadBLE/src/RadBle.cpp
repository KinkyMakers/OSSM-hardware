#include "RadBle.h"

#include <Update.h>
#include <WiFi.h>
#include <ctype.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

namespace radble {
namespace {

enum QueueKind : uint8_t {
    QUEUE_REQUEST = 0,
    QUEUE_BUTTON,
    QUEUE_ENCODER,
    QUEUE_INDICATOR,
    QUEUE_HAPTIC,
    QUEUE_AUDIO,
    QUEUE_DISPLAY,
    QUEUE_OTA_CONTROL,
    QUEUE_OTA_DATA,
};

const char* capabilityName(uint8_t bit) {
    static const char* names[] = {
        "button",       "encoder", "imu",      "power",   "analog",
        "magnetic",     "motion",  "connectivity", "indicator",
        "haptic",       "audio",   "display",  "sensorStream",
    };
    return bit < sizeof(names) / sizeof(names[0]) ? names[bit] : "unknown";
}

class WriteCallbacks final : public NimBLECharacteristicCallbacks {
   public:
    WriteCallbacks(Server* owner, uint8_t kind) : owner_(owner), kind_(kind) {}

    void onWrite(NimBLECharacteristic* characteristic,
                 NimBLEConnInfo& connection) override {
        const auto value = characteristic->getValue();
        owner_->enqueue(kind_, connection.getConnHandle(), value.data(),
                        value.size());
    }

   private:
    Server* owner_;
    uint8_t kind_;
};

uint8_t queueKindForSurface(Surface surface) {
    switch (surface) {
        case Surface::Button:
            return QUEUE_BUTTON;
        case Surface::Encoder:
            return QUEUE_ENCODER;
        case Surface::Indicator:
            return QUEUE_INDICATOR;
        case Surface::Haptic:
            return QUEUE_HAPTIC;
        case Surface::Audio:
            return QUEUE_AUDIO;
        case Surface::Display:
            return QUEUE_DISPLAY;
        default:
            return QUEUE_REQUEST;
    }
}

const char* operationForKind(uint8_t kind) {
    switch (kind) {
        case QUEUE_BUTTON:
            return "input.emit";
        case QUEUE_ENCODER:
            return "encoder.set";
        case QUEUE_INDICATOR:
            return "indicator.set";
        case QUEUE_HAPTIC:
            return "haptic.set";
        case QUEUE_AUDIO:
            return "audio.set";
        case QUEUE_DISPLAY:
            return "display.set";
        case QUEUE_OTA_CONTROL:
            return "ota.control";
        default:
            return "";
    }
}

bool deadlinePassed(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

uint16_t readU16(const uint8_t* value) {
    return static_cast<uint16_t>(value[0]) |
           (static_cast<uint16_t>(value[1]) << 8U);
}

uint32_t readU32(const uint8_t* value) {
    return static_cast<uint32_t>(value[0]) |
           (static_cast<uint32_t>(value[1]) << 8U) |
           (static_cast<uint32_t>(value[2]) << 16U) |
           (static_cast<uint32_t>(value[3]) << 24U);
}

uint32_t crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xffffffffU;
    for (size_t index = 0; index < length; ++index) {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc >> 1U) ^ (0xedb88320U &
                                 static_cast<uint32_t>(-(crc & 1U)));
    }
    return ~crc;
}

void writeU16(uint8_t* target, uint16_t value) {
    target[0] = static_cast<uint8_t>(value & 0xffU);
    target[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
}

void writeU32(uint8_t* target, uint32_t value) {
    target[0] = static_cast<uint8_t>(value & 0xffU);
    target[1] = static_cast<uint8_t>((value >> 8U) & 0xffU);
    target[2] = static_cast<uint8_t>((value >> 16U) & 0xffU);
    target[3] = static_cast<uint8_t>((value >> 24U) & 0xffU);
}

const char* surfaceName(Surface surface) {
    static const char* names[] = {
        "state",       "button", "encoder", "imu",     "power",
        "analog",      "magnetic", "motion", "connectivity",
        "indicator",   "haptic", "audio", "display",
    };
    const size_t index = static_cast<size_t>(surface);
    return index < sizeof(names) / sizeof(names[0]) ? names[index] : "state";
}

bool validSha256(const String& value) {
    if (value.length() != 64) return false;
    for (size_t index = 0; index < value.length(); ++index)
        if (!isxdigit(static_cast<unsigned char>(value[index]))) return false;
    return true;
}

String digestHex(const uint8_t* digest, size_t length) {
    static const char HEX_DIGITS[] = "0123456789abcdef";
    String output;
    output.reserve(length * 2);
    for (size_t index = 0; index < length; ++index) {
        output += HEX_DIGITS[digest[index] >> 4U];
        output += HEX_DIGITS[digest[index] & 0x0fU];
    }
    return output;
}

void restartAfterOta(void*) {
    vTaskDelay(pdMS_TO_TICKS(750));
    ESP.restart();
}

void characteristicUuid(const char* serviceUuid, const char* suffix,
                        char* output) {
    memcpy(output, serviceUuid, 36);
    memcpy(output + 19, suffix, 4);
    output[36] = '\0';
}

}  // namespace

struct Server::QueueMessage {
    uint8_t kind;
    uint16_t connectionHandle;
    uint16_t length;
    uint8_t data[MAX_MESSAGE_BYTES];
};

Result Result::success(const String& resultJson) {
    return Result{true, "", "", resultJson};
}

Result Result::failure(const char* code, const char* message) {
    return Result{false, code, message, "{}"};
}

Server::Server() = default;

bool Server::begin(NimBLEServer* server, const Config& config) {
    if (server == nullptr || config.commandHandler == nullptr ||
        config.snapshotHandler == nullptr || config.deviceType == nullptr ||
        config.serviceUuid == nullptr || strlen(config.serviceUuid) != 36 ||
        strncmp(config.serviceUuid + 18, "-0001-", 6) != 0) {
        return false;
    }

    server_ = server;
    config_ = config;
    for (size_t index = 0; index < RESPONSE_CACHE_SIZE; ++index)
        responseCacheConnections_[index] = 0xffff;
    queue_ = xQueueCreate(16, sizeof(QueueMessage));
    overflowQueue_ = xQueueCreate(16, sizeof(QueueMessage));
    if (queue_ == nullptr || overflowQueue_ == nullptr) return false;

    service_ = server_->getServiceByUUID(config_.serviceUuid);
    if (service_ == nullptr) service_ = server_->createService(config_.serviceUuid);
    if (service_ == nullptr) return false;

    char characteristicUuidValue[37];
    characteristicUuid(config_.serviceUuid, "0002", characteristicUuidValue);
    auto* protocolInfo = service_->getCharacteristic(characteristicUuidValue);
    if (protocolInfo == nullptr)
        protocolInfo = service_->createCharacteristic(
            characteristicUuidValue, NIMBLE_PROPERTY::READ, MAX_MESSAGE_BYTES);
    const String protocolInfoValue = protocolInfoJson();
    if (protocolInfoValue.length() > MAX_MESSAGE_BYTES) return false;
    protocolInfo->setValue(protocolInfoValue.c_str());

    characteristicUuid(config_.serviceUuid, "0003", characteristicUuidValue);
    auto* catalog = service_->getCharacteristic(characteristicUuidValue);
    if (catalog == nullptr)
        catalog = service_->createCharacteristic(
            characteristicUuidValue, NIMBLE_PROPERTY::READ, MAX_MESSAGE_BYTES);
    const String firstCatalogPage = catalogPageJson(0, 1);
    if (firstCatalogPage.length() > MAX_MESSAGE_BYTES) return false;
    catalog->setValue(firstCatalogPage.c_str());

    characteristicUuid(config_.serviceUuid, "1000", characteristicUuidValue);
    auto* request = service_->getCharacteristic(characteristicUuidValue);
    if (request == nullptr) {
        request = service_->createCharacteristic(
            characteristicUuidValue,
            NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
            MAX_MESSAGE_BYTES);
        request->setCallbacks(new WriteCallbacks(this, QUEUE_REQUEST));
    }

    characteristicUuid(config_.serviceUuid, "1100", characteristicUuidValue);
    responseCharacteristic_ = service_->createCharacteristic(
        characteristicUuidValue,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::INDICATE,
        MAX_MESSAGE_BYTES);
    responseCharacteristic_->setValue(
        R"({"v":1,"id":0,"stage":"completed","ok":true,"result":{"ready":true}})");

    characteristicUuid(config_.serviceUuid, "2100", characteristicUuidValue);
    eventCharacteristic_ = service_->createCharacteristic(
        characteristicUuidValue,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        MAX_MESSAGE_BYTES);
    eventCharacteristic_->setValue(R"({"event":"ready"})");

    characteristicUuid(config_.serviceUuid, "2000", characteristicUuidValue);
    surfaces_[static_cast<size_t>(Surface::State)] =
        service_->getCharacteristic(characteristicUuidValue);
    if (surfaces_[static_cast<size_t>(Surface::State)] == nullptr)
        surfaces_[static_cast<size_t>(Surface::State)] =
            service_->createCharacteristic(
                characteristicUuidValue,
                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
                MAX_MESSAGE_BYTES);

    if (config_.capabilities & CAP_SENSOR_STREAM) {
        characteristicUuid(config_.serviceUuid, "2300",
                           characteristicUuidValue);
        streamCharacteristic_ = service_->createCharacteristic(
            characteristicUuidValue,
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
            MAX_MESSAGE_BYTES);
    }

#ifndef RADBLE_OMIT_SURFACE_CHARACTERISTICS
    if (config_.createSurfaceCharacteristics) {
        if (config_.capabilities & CAP_BUTTON) {
            characteristicUuid(config_.serviceUuid, "3000",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Button,
                                        characteristicUuidValue, true);
        }
        if (config_.capabilities & CAP_ENCODER) {
            characteristicUuid(config_.serviceUuid, "3010",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Encoder,
                                        characteristicUuidValue, true);
        }
        if (config_.capabilities & CAP_IMU) {
            characteristicUuid(config_.serviceUuid, "3100",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Imu,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_POWER) {
            characteristicUuid(config_.serviceUuid, "3110",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Power,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_ANALOG) {
            characteristicUuid(config_.serviceUuid, "3120",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Analog,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_MAGNETIC) {
            characteristicUuid(config_.serviceUuid, "3130",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Magnetic,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_MOTION) {
            characteristicUuid(config_.serviceUuid, "3140",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Motion,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_CONNECTIVITY) {
            characteristicUuid(config_.serviceUuid, "3150",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Connectivity,
                                        characteristicUuidValue, false);
        }
        if (config_.capabilities & CAP_INDICATOR) {
            characteristicUuid(config_.serviceUuid, "4000",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Indicator,
                                        characteristicUuidValue, true);
        }
        if (config_.capabilities & CAP_HAPTIC) {
            characteristicUuid(config_.serviceUuid, "4010",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Haptic,
                                        characteristicUuidValue, true);
        }
        if (config_.capabilities & CAP_AUDIO) {
            characteristicUuid(config_.serviceUuid, "4020",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Audio,
                                        characteristicUuidValue, true);
        }
        if (config_.capabilities & CAP_DISPLAY) {
            characteristicUuid(config_.serviceUuid, "4030",
                               characteristicUuidValue);
            createSurfaceCharacteristic(service_, Surface::Display,
                                        characteristicUuidValue, true);
        }
    }
#endif

    characteristicUuid(config_.serviceUuid, "5000", characteristicUuidValue);
    auto* otaControl = service_->createCharacteristic(
        characteristicUuidValue,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::INDICATE,
        MAX_MESSAGE_BYTES);
    const String otaControlValue =
        config_.directOta
            ? String("{\"available\":true,\"framing\":\"v1\","
                     "\"chunkMax\":480,\"components\":[\"application\"") +
                  (config_.directFilesystemOta ? ",\"filesystem\"]}"
                                               : "]}")
            : String(R"({"available":false})");
    otaControl->setValue(otaControlValue.c_str());
    otaControl->setCallbacks(new WriteCallbacks(this, QUEUE_OTA_CONTROL));

    characteristicUuid(config_.serviceUuid, "5010", characteristicUuidValue);
    auto* otaData = service_->createCharacteristic(
        characteristicUuidValue,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR,
        MAX_MESSAGE_BYTES);
    otaData->setCallbacks(new WriteCallbacks(this, QUEUE_OTA_DATA));

    characteristicUuid(config_.serviceUuid, "5020", characteristicUuidValue);
    otaStatusCharacteristic_ = service_->createCharacteristic(
        characteristicUuidValue,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY,
        MAX_MESSAGE_BYTES);
    otaStatusCharacteristic_->setValue(
        config_.directOta ? R"({"stage":"idle","ok":true})"
                          : R"({"stage":"unavailable","ok":false})");

    auto* deviceInfo = server_->getServiceByUUID("180A");
    if (deviceInfo == nullptr) deviceInfo = server_->createService("180A");
    if (deviceInfo == nullptr) return false;
    if (deviceInfo->getCharacteristic("2A29") == nullptr)
        deviceInfo->createCharacteristic("2A29", NIMBLE_PROPERTY::READ)
            ->setValue("Research And Desire");
    if (deviceInfo->getCharacteristic("2A24") == nullptr)
        deviceInfo->createCharacteristic("2A24", NIMBLE_PROPERTY::READ)
            ->setValue(config_.deviceType);
    if (deviceInfo->getCharacteristic("2A26") == nullptr)
        deviceInfo->createCharacteristic("2A26", NIMBLE_PROPERTY::READ)
            ->setValue(config_.firmwareVersion == nullptr
                           ? "unknown"
                           : config_.firmwareVersion);

    if (config_.capabilities & CAP_POWER) {
        auto* batteryService = server_->getServiceByUUID("180F");
        if (batteryService == nullptr)
            batteryService = server_->createService("180F");
        if (batteryService == nullptr) return false;
        batteryCharacteristic_ = batteryService->getCharacteristic("2A19");
        if (batteryCharacteristic_ == nullptr) {
            batteryCharacteristic_ = batteryService->createCharacteristic(
                "2A19", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
            uint8_t initialLevel = 0;
            batteryCharacteristic_->setValue(&initialLevel, 1);
        }
    }

    refreshSnapshots();
    running_ = true;
    TaskHandle_t taskHandle = nullptr;
    if (xTaskCreate(taskEntry, "rad-ble-v1", 12288, this, 2, &taskHandle) !=
        pdPASS) {
        running_ = false;
        return false;
    }
    taskHandle_ = taskHandle;

    Serial.printf("[RAD BLE] v1 service ready for %s (%u resources)\n",
                  config_.deviceType,
                  static_cast<unsigned>(config_.resourceCount));
    return true;
}

void Server::end() {
    running_ = false;
    releaseLeaseOutputs();
    if (taskHandle_ == nullptr ||
        xTaskGetCurrentTaskHandle() == static_cast<TaskHandle_t>(taskHandle_))
        return;
    const uint32_t deadline = millis() + 2000U;
    while (taskHandle_ != nullptr && !deadlinePassed(millis(), deadline))
        vTaskDelay(pdMS_TO_TICKS(10));
    if (taskHandle_ != nullptr) {
        Serial.println("[RAD BLE] forcing server task shutdown");
        vTaskDelete(static_cast<TaskHandle_t>(taskHandle_));
        taskHandle_ = nullptr;
    }
}

const char* Server::serviceUuid() const { return config_.serviceUuid; }

NimBLECharacteristic* Server::createSurfaceCharacteristic(
    NimBLEService* service, Surface surface, const char* uuid, bool writable) {
    uint16_t properties = NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY;
    if (writable)
        properties |= NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR;
    auto* characteristic =
        service->createCharacteristic(uuid, properties, MAX_MESSAGE_BYTES);
    characteristic->setValue("[]");
    if (writable)
        characteristic->setCallbacks(
            new WriteCallbacks(this, queueKindForSurface(surface)));
    surfaces_[static_cast<size_t>(surface)] = characteristic;
    return characteristic;
}

bool Server::enqueue(uint8_t kind, uint16_t connectionHandle,
                     const uint8_t* data, size_t length) {
    if (data == nullptr || length == 0 || length > MAX_MESSAGE_BYTES ||
        queue_ == nullptr) {
        return false;
    }
    QueueMessage message{};
    message.kind = kind;
    message.connectionHandle = connectionHandle;
    message.length = static_cast<uint16_t>(length);
    memcpy(message.data, data, length);
    if (xQueueSend(static_cast<QueueHandle_t>(queue_), &message, 0) == pdTRUE)
        return true;
    return xQueueSend(static_cast<QueueHandle_t>(overflowQueue_), &message, 0) ==
           pdTRUE;
}

void Server::taskEntry(void* parameter) {
    static_cast<Server*>(parameter)->taskLoop();
}

void Server::taskLoop() {
    QueueMessage message{};
    while (running_) {
        bool handled = false;
        while (xQueueReceive(static_cast<QueueHandle_t>(queue_), &message, 0) ==
               pdTRUE) {
            dispatch(message);
            handled = true;
        }
        while (xQueueReceive(static_cast<QueueHandle_t>(overflowQueue_),
                             &message, 0) == pdTRUE) {
            if (message.kind == QUEUE_OTA_DATA) {
                publishOtaStatus(
                    R"({"stage":"failed","ok":false,"code":"busy","message":"BLE command queue is saturated"})");
                handled = true;
                continue;
            }
            JsonDocument document;
            uint32_t id = 0;
            if (deserializeJson(document, message.data, message.length) ==
                DeserializationError::Ok)
                id = document["id"] | 0U;
            sendStage(message.connectionHandle, id, "failed", false, "busy",
                      "BLE command queue is saturated", "{}", "", "");
            handled = true;
        }
        expireLease();
        expireOtaResume();
        if (millis() - lastRefreshAt_ >= 250) refreshSnapshots();
        publishScheduledStream();
        vTaskDelay(handled ? 1 : pdMS_TO_TICKS(20));
    }
    taskHandle_ = nullptr;
    vTaskDelete(nullptr);
}

void Server::dispatch(const QueueMessage& message) {
    if (message.kind == QUEUE_OTA_DATA) {
        dispatchOtaData(message);
        return;
    }
    dispatchRequest(message);
}

void Server::dispatchRequest(const QueueMessage& message) {
    JsonDocument document;
    const auto parseError =
        deserializeJson(document, message.data, message.length);
    if (parseError) {
        sendStage(message.connectionHandle, 0, "failed", false,
                  "invalid_request", "Request is not valid JSON", "{}", "", "");
        return;
    }

    if (!document["id"].is<uint32_t>()) document["id"] = esp_random();
    if (document["v"].isNull()) document["v"] = PROTOCOL_VERSION;
    if (document["op"].isNull()) document["op"] = operationForKind(message.kind);

    if (message.kind == QUEUE_OTA_CONTROL &&
        String(document["op"] | "") == "ota.control") {
        const String action = document["args"]["action"] | "";
        if (!action.isEmpty()) document["op"] = "ota." + action;
    }

    const uint32_t id = document["id"].as<uint32_t>();
    const uint8_t version = document["v"] | 0;
    String operation = document["op"] | "";
    if (version != PROTOCOL_VERSION || id == 0 || operation.isEmpty()) {
        sendStage(message.connectionHandle, id, "failed", false,
                  "invalid_request",
                  "v=1, a non-zero id, and op are required", "{}", "", "");
        return;
    }

    for (size_t index = 0; index < RESPONSE_CACHE_SIZE; ++index) {
        if (responseCacheConnections_[index] == message.connectionHandle &&
            responseCacheIds_[index] == id &&
            !responseCachePayloads_[index].isEmpty()) {
            sendSerialized(message.connectionHandle,
                           responseCachePayloads_[index], false);
            return;
        }
    }

    if (operation == "catalog.list") operation = "catalog.read";
    if (operation == "system.capabilities")
        operation = "device.capabilities";
    if (operation == "state.request") operation = "target.set";
    if (operation == "output.set" || operation == "output.pulse" ||
        operation == "output.release") {
        const String path = document["path"] | "";
        if (operation == "output.release") {
            JsonObject args = document["args"].to<JsonObject>();
            args["pattern"] = "stop";
            args["value"] = 0;
            args["brightness"] = 0;
            args["r"] = 0;
            args["g"] = 0;
            args["b"] = 0;
            args["rgb565"] = 0;
        }
        if (path.startsWith("indicator.")) operation = "indicator.set";
        else if (path.startsWith("haptic.")) operation = "haptic.set";
        else if (path.startsWith("audio.")) operation = "audio.set";
        else if (path.startsWith("display.")) operation = "display.set";
    }
    document["op"] = operation;

    Serial.printf("[RAD BLE] request conn=%u id=%lu op=%s path=%s\n",
                  message.connectionHandle, static_cast<unsigned long>(id),
                  operation.c_str(), String(document["path"] | "").c_str());
    Serial.printf("[RAD BLE] receipt id=%lu\n",
                  static_cast<unsigned long>(id));

    if (operation == "control.acquire") {
        const uint32_t now = millis();
        if (otaActive_ && otaOwner_ == message.connectionHandle) {
            sendStage(message.connectionHandle, id, "failed", false, "busy",
                      "Renew the existing lease while OTA is active", "{}",
                      "", "");
            return;
        }
        if (leaseOwner_ != 0xffff && leaseOwner_ != message.connectionHandle &&
            !deadlinePassed(now, leaseExpiresAt_)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "lease_conflict",
                      "Another client owns the control lease", "{}", "", "");
            return;
        }
        if (leaseOwner_ != 0xffff && deadlinePassed(now, leaseExpiresAt_)) {
            releaseLeaseOutputs();
            leaseOwner_ = 0xffff;
            leaseToken_ = 0;
            leaseExpiresAt_ = 0;
        }
        uint32_t ttlSeconds = document["args"]["ttl"] | 10U;
        ttlSeconds = constrain(ttlSeconds, 1U, 30U);
        leaseOwner_ = message.connectionHandle;
        leaseToken_ = esp_random();
        if (leaseToken_ == 0) leaseToken_ = 1;
        leaseExpiresAt_ = now + ttlSeconds * 1000U;
        String result = "{\"lease\":" + String(leaseToken_) +
                        ",\"ttlMs\":" + String(ttlSeconds * 1000U) + "}";
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  result, currentStateName(), currentStateName());
        publishEvent("{\"event\":\"control.lease.acquired\",\"ttlMs\":" +
                     String(ttlSeconds * 1000U) + "}");
        return;
    }

    if (operation == "control.renew") {
        const uint32_t token = document["lease"] | 0U;
        if (!hasValidLease(message.connectionHandle, token)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "lease_required", "The control lease is missing or stale",
                      "{}", "", "");
            return;
        }
        uint32_t ttlSeconds = document["args"]["ttl"] | 10U;
        ttlSeconds = constrain(ttlSeconds, 1U, 30U);
        leaseExpiresAt_ = millis() + ttlSeconds * 1000U;
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  "{\"ttlMs\":" + String(ttlSeconds * 1000U) + "}",
                  currentStateName(), currentStateName());
        return;
    }

    if (operation == "control.release") {
        const uint32_t token = document["lease"] | 0U;
        if (!hasValidLease(message.connectionHandle, token)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "lease_required", "The control lease is missing or stale",
                      "{}", "", "");
            return;
        }
        if (otaActive_ && otaOwner_ == message.connectionHandle)
            abortOta("lease_released", "OTA stopped when its lease was released");
        streamActive_ = false;
        streamRateHz_ = 0;
        releaseLeaseOutputs();
        leaseOwner_ = 0xffff;
        leaseToken_ = 0;
        leaseExpiresAt_ = 0;
        sendStage(message.connectionHandle, id, "completed", true, "", "", "{}",
                  currentStateName(), currentStateName());
        publishEvent(R"({"event":"control.lease.released"})");
        return;
    }

    if (operation == "device.capabilities") {
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  protocolInfoJson(true), "", "");
        return;
    }

    if (operation == "ota.capabilities") {
        const String result = String("{\"direct\":") +
                              (config_.directOta ? "true" : "false") +
                              ",\"network\":true,\"verified\":true,"
                              "\"framing\":\"v1\",\"chunkMax\":480,"
                              "\"components\":[\"application\"" +
                              (config_.directFilesystemOta
                                   ? ",\"filesystem\"]}"
                                   : "]}");
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  result, currentStateName(), currentStateName());
        return;
    }

    if (operation == "catalog.read") {
        size_t page = document["args"]["page"] | 0U;
        // A descriptor plus the correlated response envelope must fit in one
        // 509-byte ATT value. Verbose constraints make multi-item pages unsafe.
        const size_t pageSize = 1;
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  catalogPageJson(page, pageSize), "", "");
        return;
    }

    if (operation == "state.read") {
        const String state = stateSnapshotJson(
            config_.snapshotHandler(Surface::State, config_.context));
        // The state is already present in result. Repeating it as stateBefore
        // and stateAfter can push a valid response beyond conservative ATT
        // payloads used by some centrals.
        sendStage(message.connectionHandle, id, "completed", true, "", "", state,
                  "", "");
        return;
    }

    if (operation == "wifi.status") {
        const String status =
            config_.snapshotHandler(Surface::Connectivity, config_.context);
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  status, currentStateName(), currentStateName());
        return;
    }

    if (operation == "output.read") {
        Surface surface;
        const String path = document["path"] | "";
        bool readableOutput = false;
        for (size_t index = 0; index < config_.resourceCount; ++index) {
            const Resource& resource = config_.resources[index];
            if (path == resource.path &&
                (resource.flags & RESOURCE_READABLE) != 0) {
                readableOutput = true;
                break;
            }
        }
        if (!readableOutput || !streamSurfaceFor(path, surface) ||
            (surface != Surface::Indicator && surface != Surface::Haptic &&
             surface != Surface::Audio && surface != Surface::Display)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "unknown_path", "Output path is not readable", "{}",
                      currentStateName(), currentStateName());
            return;
        }
        const String value = config_.snapshotHandler(surface, config_.context);
        sendStage(message.connectionHandle, id, "completed", true, "", "",
                  value, currentStateName(), currentStateName());
        return;
    }

    if (operation == "sensor.readMany") {
        const JsonArrayConst paths = document["args"]["paths"].as<JsonArrayConst>();
        if (paths.isNull() || paths.size() == 0 || paths.size() > 2) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "invalid_value", "paths must contain 1..2 entries", "{}",
                      currentStateName(), currentStateName());
            return;
        }
        JsonDocument combined;
        JsonArray values = combined["values"].to<JsonArray>();
        bool allOk = true;
        for (JsonVariantConst entry : paths) {
            if (!entry.is<const char*>()) {
                allOk = false;
                continue;
            }
            JsonDocument child;
            child["v"] = PROTOCOL_VERSION;
            child["id"] = id;
            child["op"] = "sensor.read";
            child["path"] = entry.as<const char*>();
            child["args"].to<JsonObject>();
            const Result item = config_.commandHandler(
                child.as<JsonObjectConst>(), config_.context);
            JsonObject output = values.add<JsonObject>();
            output["path"] = entry.as<const char*>();
            output["ok"] = item.ok;
            if (item.ok) {
                JsonDocument parsed;
                if (!deserializeJson(parsed, item.resultJson))
                    output["result"] = parsed.as<JsonVariantConst>();
            } else {
                output["code"] = item.code;
                allOk = false;
            }
        }
        String result;
        serializeJson(combined, result);
        sendStage(message.connectionHandle, id,
                  allOk ? "completed" : "failed", allOk,
                  allOk ? "" : "unknown_path",
                  allOk ? "" : "One or more sensor paths failed", result,
                  currentStateName(), currentStateName());
        return;
    }

    if (operation == "stream.start" || operation == "stream.update" ||
        operation == "stream.stop") {
        const String before = currentStateName();
        if (streamCharacteristic_ == nullptr) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "unsupported", "Sensor streaming is unavailable", "{}",
                      before, before);
            return;
        }
        const uint32_t token = document["lease"] | 0U;
        if (!hasValidLease(message.connectionHandle, token)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "lease_required", "Acquire a control lease first", "{}",
                      before, before);
            return;
        }
        sendStage(message.connectionHandle, id, "accepted", true, "", "", "{}",
                  before, before);
        const Result result = configureStream(document.as<JsonObjectConst>());
        sendStage(message.connectionHandle, id,
                  result.ok ? "completed" : "failed", result.ok,
                  result.code.c_str(), result.message.c_str(), result.resultJson,
                  before, currentStateName());
        return;
    }

    const String before = currentStateName();
    const String requiredState = document["ifState"] | "";
    if (!requiredState.isEmpty() && requiredState != before) {
        sendStage(message.connectionHandle, id, "failed", false,
                  "invalid_state", "State precondition did not match", "{}",
                  before, before);
        return;
    }

    if (!operationIsReadOnly(operation)) {
        const uint32_t token = document["lease"] | 0U;
        if (!hasValidLease(message.connectionHandle, token)) {
            sendStage(message.connectionHandle, id, "failed", false,
                      "lease_required", "Acquire a control lease first", "{}",
                      before, before);
            return;
        }
        if (otaActive_ && !operation.startsWith("ota.")) {
            sendStage(message.connectionHandle, id, "failed", false, "busy",
                      "Direct OTA owns the device", "{}", before, before);
            return;
        }
        sendStage(message.connectionHandle, id, "accepted", true, "", "", "{}",
                  before, before);
    }

    activeOperation_ = operation;
    Result result;
    const String otaTransport = document["args"]["transport"] | "ble";
    if (operation == "ota.begin" ||
        (operation == "ota.start" && otaTransport != "wifi"))
        result = beginOta(document.as<JsonObjectConst>(),
                          message.connectionHandle);
    else if (operation == "ota.resume") {
        const uint32_t session = document["args"]["session"] | 0U;
        if (!otaActive_ || otaOwner_ != 0xffff || session != otaSession_ ||
            otaResumeExpiresAt_ == 0 ||
            deadlinePassed(millis(), otaResumeExpiresAt_)) {
            result = Result::failure("invalid_session",
                                     "No resumable OTA session matches");
        } else {
            otaOwner_ = message.connectionHandle;
            otaLeaseToken_ = document["lease"] | 0U;
            otaResumeExpiresAt_ = 0;
            result = Result::success(
                "{\"session\":" + String(otaSession_) +
                ",\"offset\":" + String(otaReceivedSize_) +
                ",\"size\":" + String(otaExpectedSize_) +
                ",\"component\":\"" + otaComponent_ + "\"}");
            publishOtaStatus("{\"stage\":\"resumed\",\"ok\":true,"
                             "\"result\":" + result.resultJson + "}");
        }
    }
    else if (operation == "ota.finish")
        result = finishOta(document.as<JsonObjectConst>(),
                           message.connectionHandle);
    else if (operation == "ota.abort") {
        abortOta("aborted", "OTA was cancelled", false);
        result = Result::success(R"({"aborted":true})");
    } else if (operation == "wifi.scan" || operation == "wifi.configure" ||
               operation == "wifi.connect" || operation == "wifi.forget") {
        result = handleWifi(document.as<JsonObjectConst>());
    } else if (operation == "system.restart") {
        if (config_.otaSafetyHandler != nullptr)
            result = config_.otaSafetyHandler(true, "restart", config_.context);
        else
            result = Result::success();
        if (result.ok &&
            xTaskCreate(restartAfterOta, "rad-restart", 2048, nullptr, 1,
                        nullptr) != pdPASS) {
            if (config_.otaSafetyHandler != nullptr)
                config_.otaSafetyHandler(false, "restart", config_.context);
            result = Result::failure("busy", "Could not schedule restart");
        } else if (result.ok) {
            result = Result::success(R"({"requested":true})");
        }
    } else
        result = config_.commandHandler(document.as<JsonObjectConst>(),
                                        config_.context);
    activeOperation_.clear();
    const String after = currentStateName();
    sendStage(message.connectionHandle, id,
              result.ok ? "completed" : "failed", result.ok,
              result.code.c_str(), result.message.c_str(), result.resultJson,
              before, after);
}

void Server::dispatchOtaData(const QueueMessage& message) {
    if (!config_.directOta) {
        publishOtaStatus(
            R"({"stage":"failed","ok":false,"code":"ota_unavailable"})");
        return;
    }
    Result result;
    if (config_.otaDataHandler != nullptr) {
        result =
            config_.otaDataHandler(message.data, message.length, config_.context);
    } else if (!otaActive_) {
        result = Result::failure("ota_not_started", "Begin an OTA session first");
    } else if (message.connectionHandle != otaOwner_ ||
               !hasValidLease(otaOwner_, otaLeaseToken_)) {
        result = Result::failure("lease_required", "The OTA control lease expired");
    } else if (message.length < 14) {
        result = Result::failure("invalid_frame", "OTA frame header is incomplete");
    } else {
        const uint32_t session = readU32(message.data);
        const uint32_t offset = readU32(message.data + 4);
        const uint16_t payloadLength = readU16(message.data + 8);
        const uint32_t expectedCrc = readU32(message.data + 10);
        const uint8_t* payload = message.data + 14;
        if (payloadLength > 480 || message.length != payloadLength + 14U)
            result = Result::failure("invalid_frame", "OTA frame length is invalid");
        else if (session != otaSession_)
            result = Result::failure("invalid_session", "OTA session does not match");
        else if (offset != otaReceivedSize_)
            result = Result::failure("invalid_offset", "OTA frame offset does not match");
        else if (otaReceivedSize_ + payloadLength > otaExpectedSize_)
            result = Result::failure("image_too_large", "OTA data exceeds declared size");
        else if (crc32(payload, payloadLength) != expectedCrc)
            result = Result::failure("crc_mismatch", "OTA frame CRC32 did not match");
        else if (Update.write(const_cast<uint8_t*>(payload), payloadLength) !=
                 payloadLength) {
            result = abortOta("write_failed", "Flash write failed", false);
        } else if (mbedtls_sha256_update_ret(
                       static_cast<mbedtls_sha256_context*>(otaShaContext_),
                       payload, payloadLength) != 0) {
            result = abortOta("sha_failed", "Could not update SHA-256", false);
        } else {
            otaReceivedSize_ += payloadLength;
            result = Result::success(
                "{\"session\":" + String(otaSession_) +
                ",\"offset\":" + String(otaReceivedSize_) +
                ",\"total\":" + String(otaExpectedSize_) + "}");
        }
    }
    JsonDocument status;
    status["stage"] = result.ok ? "progress" : "failed";
    status["ok"] = result.ok;
    if (!result.code.isEmpty()) status["code"] = result.code;
    if (!result.message.isEmpty()) status["message"] = result.message;
    if (!result.resultJson.isEmpty()) {
        JsonDocument value;
        if (!deserializeJson(value, result.resultJson))
            status["result"] = value.as<JsonVariantConst>();
    }
    String payload;
    serializeJson(status, payload);
    publishOtaStatus(payload);
}

Result Server::beginOta(JsonObjectConst request, uint16_t connectionHandle) {
    if (!config_.directOta)
        return Result::failure("ota_unavailable", "Direct OTA is unavailable");
    if (otaActive_)
        return Result::failure("busy", "An OTA session is already active");

    const size_t size = request["args"]["size"] | 0U;
    const String component = request["args"]["component"] | "application";
    String sha256 = request["args"]["sha256"] | "";
    sha256.toLowerCase();
    if (size == 0 || !validSha256(sha256))
        return Result::failure("invalid_args", "size and a SHA-256 digest are required");
    if (component != "application" &&
        !(component == "filesystem" && config_.directFilesystemOta))
        return Result::failure("unsupported_partition",
                               "Direct OTA component is unavailable");

    if (config_.otaSafetyHandler != nullptr) {
        const Result safety = config_.otaSafetyHandler(
            true, component.c_str(), config_.context);
        if (!safety.ok) return safety;
    }
    const int updateCommand =
        component == "filesystem" ? U_SPIFFS : U_FLASH;
    if (!Update.begin(size, updateCommand)) {
        if (config_.otaSafetyHandler != nullptr)
            config_.otaSafetyHandler(false, component.c_str(), config_.context);
        return Result::failure("begin_failed", "OTA partition rejected the image size");
    }
    otaComponent_ = component;

    otaShaContext_ = malloc(sizeof(mbedtls_sha256_context));
    if (otaShaContext_ == nullptr) {
        Update.abort();
        resetOta(true);
        return Result::failure("out_of_memory", "Could not create SHA-256 context");
    }
    auto* shaContext =
        static_cast<mbedtls_sha256_context*>(otaShaContext_);
    mbedtls_sha256_init(shaContext);
    if (mbedtls_sha256_starts_ret(shaContext, 0) != 0) {
        Update.abort();
        resetOta(true);
        return Result::failure("sha_failed", "Could not initialize SHA-256");
    }

    otaActive_ = true;
    otaOwner_ = connectionHandle;
    otaSession_ = esp_random();
    if (otaSession_ == 0) otaSession_ = 1;
    otaLeaseToken_ = request["lease"] | 0U;
    otaExpectedSize_ = size;
    otaReceivedSize_ = 0;
    otaExpectedSha256_ = sha256;
    const String result = "{\"session\":" + String(otaSession_) +
                          ",\"offset\":0,\"chunkMax\":480,\"size\":" +
                          String(size) + ",\"component\":\"" + component +
                          "\"}";
    publishOtaStatus("{\"stage\":\"accepted\",\"ok\":true,\"result\":" +
                     result + "}");
    Serial.printf("[RAD BLE] direct OTA session %u started (%u bytes)\n",
                  otaSession_, static_cast<unsigned>(size));
    return Result::success(result);
}

Result Server::finishOta(JsonObjectConst request, uint16_t connectionHandle) {
    if (!otaActive_)
        return Result::failure("ota_not_started", "Begin an OTA session first");
    const uint32_t session = request["args"]["session"] | 0U;
    if (connectionHandle != otaOwner_ || session != otaSession_)
        return Result::failure("invalid_session", "OTA session does not match");
    if (otaReceivedSize_ != otaExpectedSize_)
        return Result::failure("incomplete_image", "OTA image is incomplete");

    uint8_t digest[32];
    if (mbedtls_sha256_finish_ret(
            static_cast<mbedtls_sha256_context*>(otaShaContext_), digest) != 0)
        return abortOta("sha_failed", "Could not finalize SHA-256", false);
    const String actualSha256 = digestHex(digest, sizeof(digest));
    if (actualSha256 != otaExpectedSha256_)
        return abortOta("sha_mismatch", "OTA image SHA-256 did not match", false);
    if (!Update.end(false))
        return abortOta("verify_failed", "OTA image validation failed", false);

    const size_t completedSize = otaReceivedSize_;
    const String completedComponent = otaComponent_;
    resetOta(true);
    const String result = "{\"size\":" + String(completedSize) +
                          ",\"sha256\":\"" + actualSha256 +
                          "\",\"component\":\"" + completedComponent +
                          "\"}";
    publishOtaStatus(
        "{\"stage\":\"completed\",\"ok\":true,\"result\":" + result + "}");
    xTaskCreate(restartAfterOta, "rad-ble-restart", 2048, nullptr, 1, nullptr);
    return Result::success(result);
}

Result Server::abortOta(const char* code, const char* message,
                        bool publishStatus) {
    const bool wasActive = otaActive_ || Update.isRunning() ||
                           otaShaContext_ != nullptr;
    if (otaActive_ || Update.isRunning()) Update.abort();
    resetOta(wasActive);
    if (publishStatus) {
        JsonDocument status;
        status["stage"] = "failed";
        status["ok"] = false;
        status["code"] = code;
        status["message"] = message;
        String payload;
        serializeJson(status, payload);
        publishOtaStatus(payload);
    }
    return Result::failure(code, message);
}

void Server::resetOta(bool restoreSafety) {
    if (otaShaContext_ != nullptr) {
        auto* shaContext =
            static_cast<mbedtls_sha256_context*>(otaShaContext_);
        mbedtls_sha256_free(shaContext);
        free(otaShaContext_);
        otaShaContext_ = nullptr;
    }
    otaActive_ = false;
    otaOwner_ = 0xffff;
    otaSession_ = 0;
    otaLeaseToken_ = 0;
    otaExpectedSize_ = 0;
    otaReceivedSize_ = 0;
    otaExpectedSha256_.clear();
    otaResumeExpiresAt_ = 0;
    if (restoreSafety && config_.otaSafetyHandler != nullptr)
        config_.otaSafetyHandler(false, otaComponent_.c_str(), config_.context);
    otaComponent_.clear();
}

bool Server::operationIsReadOnly(const String& operation) const {
    return operation.endsWith(".read") ||
           operation == "sensor.readMany" ||
           operation == "output.read" ||
           operation == "wifi.status" ||
           operation == "ota.capabilities";
}

bool Server::streamSurfaceFor(const String& path, Surface& surface) const {
    String category = path;
    const int separator = category.indexOf('.');
    if (separator >= 0) category = category.substring(0, separator);
    if (category == "state") surface = Surface::State;
    else if (category == "button") surface = Surface::Button;
    else if (category == "encoder") surface = Surface::Encoder;
    else if (category == "imu") surface = Surface::Imu;
    else if (category == "power") surface = Surface::Power;
    else if (category == "analog") surface = Surface::Analog;
    else if (category == "magnetic") surface = Surface::Magnetic;
    else if (category == "motion") surface = Surface::Motion;
    else if (category == "lockbox" || category == "session")
        surface = Surface::Motion;
    else if (category == "connectivity") surface = Surface::Connectivity;
    else if (category == "indicator") surface = Surface::Indicator;
    else if (category == "haptic") surface = Surface::Haptic;
    else if (category == "audio") surface = Surface::Audio;
    else if (category == "display") surface = Surface::Display;
    else return false;
    // Product services may intentionally omit the optional convenience
    // characteristics (OSSM keeps legacy suffixes 3000/3010/4000). The
    // canonical stream characteristic still obtains samples through the
    // configured surface snapshot.
    return streamCharacteristic_ != nullptr ||
           surfaces_[static_cast<size_t>(surface)] != nullptr;
}

Result Server::configureStream(JsonObjectConst request) {
    const String operation = request["op"] | "";
    if (operation == "stream.stop") {
        const uint8_t stoppedId = streamId_;
        streamActive_ = false;
        streamRateHz_ = 0;
        return Result::success("{\"streamId\":" + String(stoppedId) +
                               ",\"stopped\":true}");
    }

    if (operation == "stream.update" && !streamActive_)
        return Result::failure("invalid_state", "No sensor stream is active");

    String path = request["path"] | "";
    if (path.isEmpty()) path = request["args"]["path"] | "";
    Surface requestedSurface = streamSurface_;
    if (!path.isEmpty()) {
        bool catalogedStream = false;
        for (size_t index = 0; index < config_.resourceCount; ++index) {
            if (path == config_.resources[index].path &&
                (config_.resources[index].flags & RESOURCE_STREAMABLE) != 0) {
                catalogedStream = true;
                break;
            }
        }
        if (!catalogedStream || !streamSurfaceFor(path, requestedSurface))
            return Result::failure("unknown_path", "Path is not streamable");
    }
    if (operation == "stream.start" && path.isEmpty())
        return Result::failure("invalid_value", "A stream path is required");

    uint16_t rateHz = request["args"]["rateHz"] | streamRateHz_;
    if (rateHz == 0) rateHz = 10;
    rateHz = constrain(rateHz, static_cast<uint16_t>(1),
                       static_cast<uint16_t>(100));
    if (config_.streamSafetyHandler != nullptr) {
        const Result safety = config_.streamSafetyHandler(
            path.c_str(), rateHz, config_.context);
        if (!safety.ok) return safety;
    }
    streamSurface_ = requestedSurface;
    streamRateHz_ = rateHz;
    streamBatchSize_ = 1;
    if (!streamActive_) {
        streamId_ = static_cast<uint8_t>((esp_random() % 255U) + 1U);
        streamSequence_ = 0;
        streamDropped_ = 0;
    }
    streamActive_ = true;
    nextStreamAt_ = millis();
    const String result = "{\"streamId\":" + String(streamId_) +
                          ",\"path\":\"" + path +
                          "\",\"surface\":\"" + surfaceName(streamSurface_) +
                          "\",\"rateHz\":" + String(streamRateHz_) +
                          ",\"batchSize\":1,\"encoding\":\"json-v1\"}";
    return Result::success(result);
}

Result Server::handleWifi(JsonObjectConst request) {
    if ((config_.capabilities & CAP_CONNECTIVITY) == 0)
        return Result::failure("unsupported", "Wi-Fi control is unavailable");
    const String operation = request["op"] | "";
    const JsonObjectConst args = request["args"].as<JsonObjectConst>();
    if (operation == "wifi.scan") {
        int count = WiFi.scanComplete();
        if (count == WIFI_SCAN_RUNNING)
            return Result::success(R"({"running":true})");
        if (count < 0) {
            if (WiFi.scanNetworks(true, true) == WIFI_SCAN_FAILED)
                return Result::failure("network_failed",
                                       "Could not start Wi-Fi scan");
            return Result::success(R"({"running":true,"started":true})");
        }
        JsonDocument document;
        document["running"] = false;
        document["count"] = count;
        JsonArray networks = document["networks"].to<JsonArray>();
        for (int index = 0; index < min(count, 4); ++index) {
            JsonObject network = networks.add<JsonObject>();
            network["ssid"] = WiFi.SSID(index);
            network["rssi"] = WiFi.RSSI(index);
            network["secure"] = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        }
        WiFi.scanDelete();
        String output;
        serializeJson(document, output);
        return Result::success(output);
    }
    if (operation == "wifi.forget") {
        if (!WiFi.disconnect(true, true))
            return Result::failure("storage_failed",
                                   "Could not forget Wi-Fi credentials");
        return Result::success(R"({"forgotten":true})");
    }
    const String ssid = args["ssid"] | "";
    const String password = args["password"] | "";
    if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63)
        return Result::failure("invalid_value",
                               "Wi-Fi credential lengths are invalid");
    WiFi.begin(ssid.c_str(), password.c_str());
    return Result::success(R"({"requested":true})");
}

bool Server::hasValidLease(uint16_t connectionHandle, uint32_t token) const {
    return leaseOwner_ == connectionHandle && token != 0 &&
           token == leaseToken_ && !deadlinePassed(millis(), leaseExpiresAt_);
}

void Server::expireLease() {
    if (leaseOwner_ != 0xffff && deadlinePassed(millis(), leaseExpiresAt_)) {
        if (otaActive_ && otaOwner_ == leaseOwner_)
            abortOta("lease_expired", "The OTA control lease expired");
        streamActive_ = false;
        streamRateHz_ = 0;
        releaseLeaseOutputs();
        leaseOwner_ = 0xffff;
        leaseToken_ = 0;
        leaseExpiresAt_ = 0;
        publishEvent(R"({"event":"control.lease.expired"})");
    }
}

void Server::releaseLeaseOutputs() {
    if (config_.leaseReleaseHandler != nullptr)
        config_.leaseReleaseHandler(config_.context);
}

void Server::expireOtaResume() {
    if (otaActive_ && otaOwner_ == 0xffff && otaResumeExpiresAt_ != 0 &&
        deadlinePassed(millis(), otaResumeExpiresAt_))
        abortOta("resume_timeout", "The OTA resume window expired");
}

void Server::onConnect(uint16_t connectionHandle) {
    JsonDocument event;
    event["event"] = "ble.connected";
    event["connection"] = connectionHandle;
    String payload;
    serializeJson(event, payload);
    publishEvent(payload);
}

void Server::onDisconnect(uint16_t connectionHandle) {
    if (otaActive_ && otaOwner_ == connectionHandle) {
        otaOwner_ = 0xffff;
        otaLeaseToken_ = 0;
        otaResumeExpiresAt_ = millis() + 60000U;
        publishOtaStatus("{\"stage\":\"paused\",\"ok\":true,"
                         "\"result\":{\"session\":" + String(otaSession_) +
                         ",\"offset\":" + String(otaReceivedSize_) +
                         ",\"resumeTtlMs\":60000}}");
    }
    if (leaseOwner_ == connectionHandle) {
        streamActive_ = false;
        streamRateHz_ = 0;
        releaseLeaseOutputs();
        leaseOwner_ = 0xffff;
        leaseToken_ = 0;
        leaseExpiresAt_ = 0;
    }
    for (size_t index = 0; index < RESPONSE_CACHE_SIZE; ++index) {
        if (responseCacheConnections_[index] == connectionHandle) {
            responseCacheConnections_[index] = 0xffff;
            responseCacheIds_[index] = 0;
            responseCachePayloads_[index].clear();
        }
    }
}

void Server::publishEvent(const String& eventJson) {
    if (eventCharacteristic_ == nullptr) return;
    eventCharacteristic_->setValue(eventJson.c_str());
    eventCharacteristic_->notify();
    Serial.printf("[RAD BLE] event %s\n", eventJson.c_str());
}

void Server::publishStream(const uint8_t* data, size_t length) {
    if (streamCharacteristic_ == nullptr || data == nullptr ||
        length > MAX_MESSAGE_BYTES)
        return;
    streamCharacteristic_->setValue(data, length);
    streamCharacteristic_->notify();
}

void Server::publishOtaStatus(const String& statusJson) {
    if (otaStatusCharacteristic_ == nullptr) return;
    otaStatusCharacteristic_->setValue(statusJson.c_str());
    otaStatusCharacteristic_->notify();
    Serial.printf("[RAD BLE] ota %s\n", statusJson.c_str());
}

void Server::refreshSnapshots() {
    lastRefreshAt_ = millis();
    for (size_t index = 0; index < static_cast<size_t>(Surface::Count);
         ++index) {
        auto* characteristic = surfaces_[index];
        if (characteristic == nullptr) continue;
        const String deviceValue = config_.snapshotHandler(
            static_cast<Surface>(index), config_.context);
        if (deviceValue.isEmpty()) continue;
        String value = deviceValue;
        bool forceNotify = false;
        if (index == static_cast<size_t>(Surface::State)) {
            const bool changed = deviceValue != lastDeviceStateSnapshot_;
            if (changed) {
                String previousState;
                String nextState;
                JsonDocument previous;
                JsonDocument next;
                if (!deserializeJson(previous, lastDeviceStateSnapshot_))
                    previousState = previous["state"] | "";
                if (!deserializeJson(next, deviceValue))
                    nextState = next["state"] | "";
                ++stateSequence_;
                lastDeviceStateSnapshot_ = deviceValue;
                if (!previousState.isEmpty() && previousState != nextState) {
                    JsonDocument event;
                    event["event"] = "state.changed";
                    event["stateBefore"] = previousState;
                    event["stateAfter"] = nextState;
                    event["sequence"] = stateSequence_;
                    String eventPayload;
                    serializeJson(event, eventPayload);
                    publishEvent(eventPayload);
                }
            }
            value = stateSnapshotJson(deviceValue);
            forceNotify = changed ||
                          millis() - lastStateHeartbeatAt_ >= 1000U;
            if (forceNotify) lastStateHeartbeatAt_ = millis();
        }
        if (value.length() > MAX_MESSAGE_BYTES) continue;
        const auto previous = characteristic->getValue();
        const bool changed =
            previous.size() != value.length() ||
            memcmp(previous.data(), value.c_str(), value.length()) != 0;
        if (!changed && !forceNotify) continue;
        characteristic->setValue(value.c_str());
        if (server_ != nullptr && server_->getConnectedCount() > 0 &&
            characteristic->getHandle() != 0)
            characteristic->notify();

        if (index == static_cast<size_t>(Surface::Power) &&
            batteryCharacteristic_ != nullptr) {
            JsonDocument power;
            if (!deserializeJson(power, deviceValue)) {
                int percent = 0;
                if (!power["percent"].isNull())
                    percent = power["percent"].as<int>();
                else if (!power["batteryPercent"].isNull())
                    percent = power["batteryPercent"].as<int>();
                percent = constrain(percent, 0, 100);
                const uint8_t level = static_cast<uint8_t>(percent);
                const auto prior = batteryCharacteristic_->getValue();
                if (prior.size() != 1 || prior[0] != level) {
                    batteryCharacteristic_->setValue(&level, 1);
                    if (server_ != nullptr && server_->getConnectedCount() > 0 &&
                        batteryCharacteristic_->getHandle() != 0)
                        batteryCharacteristic_->notify();
                }
            }
        }
    }
}

void Server::publishScheduledStream() {
    if (!streamActive_ || streamCharacteristic_ == nullptr ||
        streamRateHz_ == 0 || !deadlinePassed(millis(), nextStreamAt_))
        return;
    const uint32_t interval = max(10U, 1000U / streamRateHz_);
    nextStreamAt_ = millis() + interval;
    const String sample = config_.snapshotHandler(streamSurface_, config_.context);
    if (sample.isEmpty() || sample.length() + STREAM_HEADER_BYTES >
                                MAX_MESSAGE_BYTES) {
        ++streamDropped_;
        return;
    }
    uint8_t frame[MAX_MESSAGE_BYTES]{};
    frame[0] = PROTOCOL_VERSION;
    frame[1] = streamId_;
    writeU16(frame + 2, 1U);  // bit 0: UTF-8 JSON payload
    writeU16(frame + 4, 1U);
    writeU16(frame + 6, static_cast<uint16_t>(sample.length()));
    writeU32(frame + 8, streamSequence_++);
    writeU32(frame + 12, millis());
    writeU32(frame + 16, streamDropped_);
    memcpy(frame + STREAM_HEADER_BYTES, sample.c_str(), sample.length());
    streamCharacteristic_->setValue(frame, STREAM_HEADER_BYTES + sample.length());
    if (!streamCharacteristic_->notify()) ++streamDropped_;
}

String Server::stateSnapshotJson(const String& deviceSnapshot) {
    JsonDocument device;
    JsonDocument output;
    if (!deserializeJson(device, deviceSnapshot)) {
        const JsonObjectConst object = device.as<JsonObjectConst>();
        for (JsonPairConst pair : object)
            output[pair.key()] = pair.value();
        if (output["state"].isNull()) output["state"] = "unknown";
    } else {
        output["state"] = "unknown";
    }
    output["v"] = PROTOCOL_VERSION;
    output["sequence"] = stateSequence_;
    output["uptimeMs"] = millis();
    output["activeOperation"] = activeOperation_;
    JsonObject lease = output["lease"].to<JsonObject>();
    lease["active"] = leaseOwner_ != 0xffff &&
                      !deadlinePassed(millis(), leaseExpiresAt_);
    if (lease["active"].as<bool>()) {
        lease["owner"] = leaseOwner_;
        lease["expiresInMs"] = leaseExpiresAt_ - millis();
    }
    String value;
    serializeJson(output, value);
    return value;
}

String Server::currentStateName() const {
    const String snapshot =
        config_.snapshotHandler(Surface::State, config_.context);
    JsonDocument document;
    if (deserializeJson(document, snapshot) == DeserializationError::Ok)
        return document["state"] | "";
    return "";
}

String Server::protocolInfoJson(bool compact) const {
    JsonDocument document;
    document["protocol"] = "rad-ble";
    document["version"] = PROTOCOL_VERSION;
    document["deviceType"] = config_.deviceType;
    document["serviceUuid"] = config_.serviceUuid;
    document["security"] = "open";
    document["directOta"] = config_.directOta;
    document["directFilesystemOta"] = config_.directFilesystemOta;
    if (!compact) {
        document["deviceName"] = config_.deviceName == nullptr
                                     ? config_.deviceType
                                     : config_.deviceName;
        document["firmwareVersion"] = config_.firmwareVersion == nullptr
                                          ? "unknown"
                                          : config_.firmwareVersion;
        String build = config_.build == nullptr ? "unknown" : config_.build;
        if (build.length() > 12) build.remove(12);
        document["build"] = build;
        document["maxMtu"] = 512;
        document["maxMessageBytes"] = MAX_MESSAGE_BYTES;
        document["stateHeartbeatMs"] = 1000;
        document["otaResumeTtlMs"] = 60000;
        document["streamHeaderBytes"] = STREAM_HEADER_BYTES;
    }
    uint32_t capabilityHash = 2166136261U;
    for (size_t index = 0; index < config_.resourceCount; ++index) {
        const Resource& resource = config_.resources[index];
        for (const char* cursor = resource.path; cursor != nullptr && *cursor;
             ++cursor) {
            capabilityHash ^= static_cast<uint8_t>(*cursor);
            capabilityHash *= 16777619U;
        }
        capabilityHash ^= resource.flags;
        capabilityHash *= 16777619U;
    }
    char hash[9];
    snprintf(hash, sizeof(hash), "%08lx",
             static_cast<unsigned long>(capabilityHash));
    document["capabilityHash"] = hash;
    JsonArray capabilities = document["capabilities"].to<JsonArray>();
    for (uint8_t bit = 0; bit < 13; ++bit) {
        if (config_.capabilities & (1UL << bit))
            capabilities.add(capabilityName(bit));
    }
    String output;
    serializeJson(document, output);
    return output;
}

String Server::catalogPageJson(size_t page, size_t pageSize) const {
    JsonDocument document;
    document["page"] = page;
    document["pageSize"] = pageSize;
    document["total"] = config_.resourceCount;
    document["pages"] =
        (config_.resourceCount + pageSize - 1) / pageSize;
    JsonArray resources = document["resources"].to<JsonArray>();
    const size_t begin = page * pageSize;
    const size_t end = min(begin + pageSize, config_.resourceCount);
    for (size_t index = begin; index < end; ++index) {
        const Resource& resource = config_.resources[index];
        JsonObject item = resources.add<JsonObject>();
        item["id"] = resource.id;
        item["path"] = resource.path;
        item["category"] = resource.category;
        item["type"] = resource.valueType;
        if (resource.units != nullptr && resource.units[0] != '\0')
            item["units"] = resource.units;
        item["readable"] = (resource.flags & RESOURCE_READABLE) != 0;
        item["writable"] = (resource.flags & RESOURCE_WRITABLE) != 0;
        item["streamable"] = (resource.flags & RESOURCE_STREAMABLE) != 0;
        item["persistent"] = (resource.flags & RESOURCE_PERSISTENT) != 0;
        item["leaseRequired"] =
            (resource.flags & RESOURCE_LEASE_REQUIRED) != 0;
        item["safetyCritical"] =
            (resource.flags & RESOURCE_SAFETY_CRITICAL) != 0;
        item["available"] = (resource.flags & RESOURCE_AVAILABLE) != 0;
        if (resource.constraintsJson != nullptr &&
            resource.constraintsJson[0] != '\0') {
            JsonDocument constraints;
            if (!deserializeJson(constraints, resource.constraintsJson))
                item["constraints"] = constraints.as<JsonVariantConst>();
        }
    }
    String output;
    serializeJson(document, output);
    return output;
}

void Server::sendStage(uint16_t connectionHandle, uint32_t id,
                       const char* stage, bool ok, const char* code,
                       const char* message, const String& resultJson,
                       const String& stateBefore, const String& stateAfter) {
    JsonDocument document;
    document["v"] = PROTOCOL_VERSION;
    document["id"] = id;
    document["stage"] = stage;
    document["ok"] = ok;
    if (code != nullptr && code[0] != '\0')
        document["code"] = code;
    else if (ok && strcmp(stage, "completed") == 0)
        document["code"] = "ok";
    if (message != nullptr && message[0] != '\0') document["message"] = message;
    if (!stateBefore.isEmpty()) document["stateBefore"] = stateBefore;
    if (!stateAfter.isEmpty()) document["stateAfter"] = stateAfter;

    String payload;
    serializeJson(document, payload);
    if (!resultJson.isEmpty() && payload.endsWith("}")) {
        // Result JSON is produced by trusted firmware handlers and is already
        // serialized. Embedding it avoids a second dynamic JsonDocument, which
        // can fail on memory-constrained devices and silently omit lease or
        // catalog data from an otherwise successful response.
        payload.remove(payload.length() - 1);
        payload += ",\"result\":";
        payload += resultJson;
        payload += "}";
    }
    if (payload.length() > MAX_MESSAGE_BYTES) {
        payload = "{\"v\":1,\"id\":" + String(id) +
                  ",\"stage\":\"failed\",\"ok\":false,"
                  "\"code\":\"response_too_large\"}";
    }
    sendSerialized(connectionHandle, payload,
                   strcmp(stage, "completed") == 0 ||
                       strcmp(stage, "failed") == 0);
    if (strcmp(stage, "completed") == 0 || strcmp(stage, "failed") == 0)
        Serial.printf("[RAD BLE] terminal id=%lu stage=%s ok=%u code=%s\n",
                      static_cast<unsigned long>(id), stage, ok ? 1U : 0U,
                      code != nullptr && code[0] != '\0' ? code
                                                         : (ok ? "ok" : ""));
}

void Server::sendSerialized(uint16_t connectionHandle, const String& payload,
                            bool remember) {
    if (responseCharacteristic_ == nullptr) return;
    responseCharacteristic_->setValue(payload.c_str());
    // NimBLE permits only one outstanding indication per connection. A
    // mutation can emit accepted and completed back-to-back, and independent
    // clients may pipeline requests, so wait briefly for the preceding
    // acknowledgement instead of silently dropping the correlated result.
    bool delivered = false;
    for (uint16_t attempt = 0; attempt < 200; ++attempt) {
        if (responseCharacteristic_->indicate(
                reinterpret_cast<const uint8_t*>(payload.c_str()),
                payload.length(), connectionHandle)) {
            delivered = true;
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    if (!delivered)
        Serial.printf("[RAD BLE] response delivery timed out for connection %u\n",
                      connectionHandle);
    if (remember) {
        JsonDocument document;
        if (!deserializeJson(document, payload)) {
            responseCacheConnections_[responseCacheNext_] = connectionHandle;
            responseCacheIds_[responseCacheNext_] = document["id"] | 0U;
            responseCachePayloads_[responseCacheNext_] = payload;
            responseCacheNext_ = (responseCacheNext_ + 1) % RESPONSE_CACHE_SIZE;
        }
    }
    Serial.printf("[RAD BLE] response %s\n", payload.c_str());
}

}  // namespace radble
