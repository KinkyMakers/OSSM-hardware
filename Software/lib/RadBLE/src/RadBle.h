#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>

#include <cstddef>
#include <cstdint>

namespace radble {

inline constexpr char OSSM_SERVICE_UUID[] =
    "522b443a-4f53-534d-0001-420badbabe69";
inline constexpr char RADR_SERVICE_UUID[] =
    "522b443a-5241-4452-0001-420badbabe69";
inline constexpr char DTT_SERVICE_UUID[] =
    "522b443a-4454-5400-0001-420badbabe69";
inline constexpr char LKBX_SERVICE_UUID[] =
    "522b443a-4c4b-4258-0001-420badbabe69";

inline constexpr uint8_t PROTOCOL_VERSION = 1;
// An ATT MTU of 512 leaves 509 bytes for a notification or indication value.
// Keeping every queued JSON message at or below that limit avoids relying on
// long-read behavior for asynchronous responses.
inline constexpr size_t MAX_MESSAGE_BYTES = 509;
inline constexpr size_t STREAM_HEADER_BYTES = 20;

enum class Surface : uint8_t {
    State,
    Button,
    Encoder,
    Imu,
    Power,
    Analog,
    Magnetic,
    Motion,
    Connectivity,
    Indicator,
    Haptic,
    Audio,
    Display,
    Count,
};

enum Capability : uint32_t {
    CAP_BUTTON = 1UL << 0,
    CAP_ENCODER = 1UL << 1,
    CAP_IMU = 1UL << 2,
    CAP_POWER = 1UL << 3,
    CAP_ANALOG = 1UL << 4,
    CAP_MAGNETIC = 1UL << 5,
    CAP_MOTION = 1UL << 6,
    CAP_CONNECTIVITY = 1UL << 7,
    CAP_INDICATOR = 1UL << 8,
    CAP_HAPTIC = 1UL << 9,
    CAP_AUDIO = 1UL << 10,
    CAP_DISPLAY = 1UL << 11,
    CAP_SENSOR_STREAM = 1UL << 12,
};

enum ResourceFlag : uint16_t {
    RESOURCE_READABLE = 1U << 0,
    RESOURCE_WRITABLE = 1U << 1,
    RESOURCE_STREAMABLE = 1U << 2,
    RESOURCE_PERSISTENT = 1U << 3,
    RESOURCE_LEASE_REQUIRED = 1U << 4,
    RESOURCE_SAFETY_CRITICAL = 1U << 5,
    RESOURCE_AVAILABLE = 1U << 6,
    RESOURCE_EXPERIMENTAL = 1U << 7,
};

struct Resource {
    const char* id;
    const char* path;
    const char* category;
    const char* valueType;
    const char* units;
    uint16_t flags;
    const char* constraintsJson;
};

struct Result {
    bool ok;
    String code;
    String message;
    String resultJson;

    static Result success(const String& resultJson = "{}");
    static Result failure(const char* code, const char* message);
};

using CommandHandler = Result (*)(JsonObjectConst request, void* context);
using SnapshotHandler = String (*)(Surface surface, void* context);
using OtaDataHandler =
    Result (*)(const uint8_t* data, size_t length, void* context);
using OtaSafetyHandler = Result (*)(bool starting, const char* component,
                                    void* context);
using LeaseReleaseHandler = void (*)(void* context);
using StreamSafetyHandler = Result (*)(const char* path, uint16_t rateHz,
                                       void* context);

struct Config {
    const char* deviceType;
    const char* deviceName;
    const char* serviceUuid;
    const char* firmwareVersion;
    const char* build;
    uint32_t capabilities;
    const Resource* resources;
    size_t resourceCount;
    CommandHandler commandHandler;
    SnapshotHandler snapshotHandler;
    OtaDataHandler otaDataHandler;
    void* context;
    bool directOta;
    bool directFilesystemOta;
    OtaSafetyHandler otaSafetyHandler;
    LeaseReleaseHandler leaseReleaseHandler;
    // OSSM already owns suffixes 1000, 2000, 3000, 3010, and 4000. It reuses
    // its request/state characteristics and keeps its legacy optional surface
    // characteristics instead of replacing them.
    bool createSurfaceCharacteristics;
    // Optional product radio/sensor arbitration before a stream starts.
    StreamSafetyHandler streamSafetyHandler;
};

class Server {
   public:
    Server();

    bool begin(NimBLEServer* server, const Config& config);
    void end();
    void onConnect(uint16_t connectionHandle);
    void onDisconnect(uint16_t connectionHandle);
    const char* serviceUuid() const;

    void publishEvent(const String& eventJson);
    void publishStream(const uint8_t* data, size_t length);
    void publishOtaStatus(const String& statusJson);

    // Public only for the fixed NimBLE callback shims. Device code should use
    // the typed handlers configured in begin().
    bool enqueue(uint8_t kind, uint16_t connectionHandle,
                 const uint8_t* data, size_t length);

   private:
    struct QueueMessage;
    static void taskEntry(void* parameter);
    void taskLoop();
    void dispatch(const QueueMessage& message);
    void dispatchRequest(const QueueMessage& message);
    void dispatchOtaData(const QueueMessage& message);
    Result beginOta(JsonObjectConst request, uint16_t connectionHandle);
    Result finishOta(JsonObjectConst request, uint16_t connectionHandle);
    Result abortOta(const char* code, const char* message,
                    bool publishStatus = true);
    void resetOta(bool restoreSafety);
    void refreshSnapshots();
    void publishScheduledStream();
    void expireLease();
    void releaseLeaseOutputs();
    void expireOtaResume();

    String protocolInfoJson(bool compact = false) const;
    String catalogPageJson(size_t page, size_t pageSize) const;
    String stateSnapshotJson(const String& deviceSnapshot);
    String currentStateName() const;
    Result configureStream(JsonObjectConst request);
    Result handleWifi(JsonObjectConst request);
    bool streamSurfaceFor(const String& path, Surface& surface) const;
    void sendStage(uint16_t connectionHandle, uint32_t id, const char* stage,
                   bool ok, const char* code, const char* message,
                   const String& resultJson, const String& stateBefore,
                   const String& stateAfter);
    void sendSerialized(uint16_t connectionHandle, const String& payload,
                        bool remember);
    bool hasValidLease(uint16_t connectionHandle, uint32_t token) const;
    bool operationIsReadOnly(const String& operation) const;
    NimBLECharacteristic* createSurfaceCharacteristic(
        NimBLEService* service, Surface surface, const char* uuid,
        bool writable);

    Config config_{};
    NimBLEServer* server_ = nullptr;
    NimBLEService* service_ = nullptr;
    NimBLECharacteristic* responseCharacteristic_ = nullptr;
    NimBLECharacteristic* eventCharacteristic_ = nullptr;
    NimBLECharacteristic* streamCharacteristic_ = nullptr;
    NimBLECharacteristic* otaStatusCharacteristic_ = nullptr;
    NimBLECharacteristic* batteryCharacteristic_ = nullptr;
    NimBLECharacteristic* surfaces_[static_cast<size_t>(Surface::Count)]{};
    void* queue_ = nullptr;
    void* overflowQueue_ = nullptr;
    void* volatile taskHandle_ = nullptr;
    volatile bool running_ = false;
    uint16_t leaseOwner_ = 0xffff;
    uint32_t leaseToken_ = 0;
    uint32_t leaseExpiresAt_ = 0;
    static constexpr size_t RESPONSE_CACHE_SIZE = 4;
    uint16_t responseCacheConnections_[RESPONSE_CACHE_SIZE]{};
    uint32_t responseCacheIds_[RESPONSE_CACHE_SIZE]{};
    String responseCachePayloads_[RESPONSE_CACHE_SIZE];
    size_t responseCacheNext_ = 0;
    uint32_t lastRefreshAt_ = 0;
    uint32_t lastStateHeartbeatAt_ = 0;
    uint32_t stateSequence_ = 0;
    String lastDeviceStateSnapshot_;
    String activeOperation_;
    bool streamActive_ = false;
    Surface streamSurface_ = Surface::State;
    uint8_t streamId_ = 0;
    uint16_t streamRateHz_ = 0;
    uint16_t streamBatchSize_ = 1;
    uint32_t streamSequence_ = 0;
    uint32_t streamDropped_ = 0;
    uint32_t nextStreamAt_ = 0;
    bool otaActive_ = false;
    uint16_t otaOwner_ = 0xffff;
    uint32_t otaSession_ = 0;
    uint32_t otaLeaseToken_ = 0;
    size_t otaExpectedSize_ = 0;
    size_t otaReceivedSize_ = 0;
    String otaExpectedSha256_;
    String otaComponent_;
    void* otaShaContext_ = nullptr;
    uint32_t otaResumeExpiresAt_ = 0;
};

}  // namespace radble
