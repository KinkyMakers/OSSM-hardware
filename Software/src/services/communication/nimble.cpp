#include "nimble.h"

#include <ArduinoJson.h>
#include <constants/UserConfig.h>
#include <services/tasks.h>

#include <regex>

#include "command/commands.hpp"

#define SERVICE_UUID "522b443a-4f53-534d-0001-420badbabe69"

// Clients should write to this char.
#define CHARACTERISTIC_UUID "522b443a-4f53-534d-0002-420badbabe69"

// Clients should read the current state from this char.
#define CHARACTERISTIC_STATE_UUID "522b443a-4f53-534d-0010-420badbabe69"

// this is a list of all the patterns on the device.
#define CHARACTERISTIC_PATTERNS_UUID "522b443a-4f53-534d-0100-420badbabe69"

const char* MANUFACTURER_NAME_UUID =
    "2A29";                           // Standard UUID for manufacturer name
const char* SYSTEM_ID_UUID = "2A23";  // Standard UUID for system ID
const char* DEVICE_INFO_SERVICE_UUID = "180A";  // Device Information Service

// Define the global variables
NimBLEServer* pServer = nullptr;
uint16_t nimbleTargetPosition = 0;
int16_t nimbleTargetVelocity = 0;

// Queue to store target positions and velocities
std::queue<Target> targetQueue = std::queue<Target>();
const size_t MAX_QUEUE_SIZE = 2048;

// Add a new target to the queue
bool enqueueTarget(uint16_t position, int16_t velocity) {
    if (targetQueue.size() >= MAX_QUEUE_SIZE) {
        return false;  // Queue is full
    }

    targetQueue.push(Target{position, velocity});
    return true;
}

// Clear all targets from queue
void clearQueue() { std::queue<Target>().swap(targetQueue); }

// Add at the top with other global variables
NimBLECharacteristic* pCharacteristic =
    nullptr;  // Global pointer to B2 characteristic

/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    uint32_t lastWriteTime = 0;
    float writeHz = 0;
    const float alpha = 0.1;  // Smoothing factor for exponential moving average

    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();

        // Convert the received value to a std::string for easier parsing
        std::string cmd(value.begin(), value.end());
        bool matched = false;

        // Regex patterns for set and go commands
        std::regex set_regex(
            R"(set:(speed|stroke|depth|sensation|pattern):([0-9]{1,3}))");
        std::regex go_regex(R"(go:(simplePenetration|strokeEngine|menu))");
        std::smatch match;

        bool isSet = std::regex_match(cmd, match, set_regex);
        bool isGo = std::regex_match(cmd, match, go_regex);

        if (isSet || isGo) {
            ESP_LOGD("NIMBLE", "OK command: %s", cmd.c_str());
            ossmInterface->ble_click(String(cmd.c_str()));
            matched = true;
        } else {
            ESP_LOGD("NIMBLE", "FAIL command: %s", cmd.c_str());
        }

        if (matched) {
            pCharacteristic->setValue("ok:" + String(cmd.c_str()));
        } else {
            pCharacteristic->setValue("fail:" + String(cmd.c_str()));
        }
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV("NIMBLE", "Notification/Indication return code: %d, %s", code,
                 NimBLEUtils::returnCodeToString(code));
    }
} chrCallbacks;

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
} dscCallbacks;

/** Handler class for server actions */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        ESP_LOGI("NIMBLE", "Client connected: %s",
                 connInfo.getAddress().toString().c_str());
        ESP_LOGI("NIMBLE", "Connection count: %d",
                 pServer->getConnectedCount());
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                      int reason) override {
        ESP_LOGI("NIMBLE", "Client disconnected: %s, reason: %d",
                 connInfo.getAddress().toString().c_str(), reason);
        ESP_LOGI("NIMBLE", "Connection count: %d",
                 pServer->getConnectedCount());

        // Restart advertising when client disconnects
        if (pServer->getConnectedCount() == 0) {
            ESP_LOGI("NIMBLE",
                     "No connections remaining, restarting advertising");
            pServer->startAdvertising();
        }
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        ESP_LOGD("NIMBLE", "MTU changed to: %d for connection: %s", MTU,
                 connInfo.getAddress().toString().c_str());
    }
} serverCallbacks;

void nimbleLoop(void* pvParameters) {
    NimBLEServer* pServer = (NimBLEServer*)pvParameters;
    /** Loop here and send notifications to connected peers */

    String lastState = "";
    int lastConnCount = 0;
    int lastMessageTime = 0;
    while (true) {
        // Check if we should be advertising (no connections)
        if (pServer->getConnectedCount() == 0) {
            // If not advertising and no connections, restart advertising
            if (!pServer->getAdvertising()) {
                ESP_LOGI("NIMBLE",
                         "No connections and not advertising, restarting "
                         "advertising");
                pServer->startAdvertising();
            }
            vTaskDelay(pdMS_TO_TICKS(2000));
            continue;
        }

        String currentState = ossmInterface->getCurrentState();
        int currentConnCount = pServer->getConnectedCount();

        // Clear last state when connection count changes
        if (currentConnCount != lastConnCount) {
            lastState = "";
            lastConnCount = currentConnCount;
        }

        NimBLEService* pSvc = pServer->getServiceByUUID(SERVICE_UUID);
        if (!pSvc) {
            lastState = "";
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        NimBLECharacteristic* pChr =
            pSvc->getCharacteristic(CHARACTERISTIC_STATE_UUID);
        if (!pChr) {
            lastState = "";
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int currentTime = millis();
        bool stateChanged = currentState != lastState;
        bool timeElapsed = (currentTime - lastMessageTime) > 1000;

        if (!stateChanged && !timeElapsed) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        lastMessageTime = currentTime;

        if (stateChanged) {
            ESP_LOGD("NIMBLE", "State changed to: %s", currentState.c_str());
        }
        pChr->setValue(currentState);
        pChr->notify();
        lastState = currentState;
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

void initNimble() {
    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("OSSM");

    /**
     * Set the IO capabilities of the device, each option will trigger a
     * different pairing method. BLE_HS_IO_DISPLAY_ONLY    - Passkey pairing
     *  BLE_HS_IO_DISPLAY_YESNO   - Numeric comparison pairing
     *  BLE_HS_IO_NO_INPUT_OUTPUT - DEFAULT setting - just works pairing
     */
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_ONLY); // use passkey
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO); //use numeric
    // comparison

    /**
     *  2 different ways to set security - both calls achieve the same result.
     *  no bonding, no man in the middle protection, BLE secure connections.
     *
     *  These are the default values, only shown here for demonstration.
     */
    // NimBLEDevice::setSecurityAuth(false, false, true);

    NimBLEDevice::setSecurityAuth(
        /*BLE_SM_PAIR_AUTHREQ_BOND | BLE_SM_PAIR_AUTHREQ_MITM |*/
        BLE_SM_PAIR_AUTHREQ_SC);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    // Create Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    // Command characteristic (writable, readable)
    NimBLECharacteristic* pChar = pService->createCharacteristic(
        CHARACTERISTIC_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::READ);

    // Store the characteristic pointer globally
    pCharacteristic = pChar;

    pChar->setCallbacks(&chrCallbacks);

    // State characteristic (read/notify string payload for state)
    NimBLECharacteristic* pStateChar = pService->createCharacteristic(
        CHARACTERISTIC_STATE_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
    pStateChar->setValue("ok:boot");

    // Patterns characteristic (read-only list of all patterns)
    NimBLECharacteristic* pPatternsChar = pService->createCharacteristic(
        CHARACTERISTIC_PATTERNS_UUID, NIMBLE_PROPERTY::READ);
    // Use ArduinoJson to construct the patterns JSON
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    for (int i = 0; i < sizeof(UserConfig::language.StrokeEngineNames) /
                            sizeof(UserConfig::language.StrokeEngineNames[0]);
         i++) {
        JsonObject pattern = arr.createNestedObject();
        pattern["name"] = UserConfig::language.StrokeEngineNames[i];
        pattern["idx"] = i;
    }

    String jsonString;
    serializeJson(arr, jsonString);
    pPatternsChar->setValue(jsonString.c_str());

    // Start the services
    pService->start();

    // Add Device Information Service
    NimBLEService* pDeviceInfoService =
        pServer->createService(DEVICE_INFO_SERVICE_UUID);

    // Add Manufacturer Name characteristic
    NimBLECharacteristic* pManufacturerName =
        pDeviceInfoService->createCharacteristic(MANUFACTURER_NAME_UUID,
                                                 NIMBLE_PROPERTY::READ);
    pManufacturerName->setValue("Research And Desire");

    // Add System ID characteristic
    NimBLECharacteristic* pSystemId = pDeviceInfoService->createCharacteristic(
        SYSTEM_ID_UUID, NIMBLE_PROPERTY::READ);
    uint8_t systemId[] = {0x88, 0x1A, 0x14, 0xFF, 0xFE, 0x34, 0x29, 0x63};
    pSystemId->setValue(systemId, 8);

    // Start the device info service
    pDeviceInfoService->start();

    // Update advertising to include new services
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("OSSM");
    pAdvertising->addServiceUUID(pService->getUUID());
    pAdvertising->addServiceUUID(pDeviceInfoService->getUUID());
    pAdvertising->enableScanResponse(true);

    // Configure advertising parameters for better reliability
    pAdvertising->setMinInterval(0x20);  // 20ms minimum interval
    pAdvertising->setMaxInterval(0x40);  // 40ms maximum interval

    pAdvertising->start();

    xTaskCreatePinnedToCore(
        nimbleLoop, "nimbleLoop", 9 * configMINIMAL_STACK_SIZE, pServer,
        configMAX_PRIORITIES - 1, nullptr, Tasks::stepperCore);
}
