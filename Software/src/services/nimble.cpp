#include "nimble.h"

#include "tasks.h"

const char* SERVICE_A_UUID = "1D14D6EE-FD63-4FA1-BFA4-8F47B42119F0";
const char* CHARACTERISTIC_A1_UUID = "F7BF3564-FB6D-4E53-88A4-5E37E0326063";
const char* CHARACTERISTIC_A2_UUID = "984227F3-34FC-4045-A5D0-2C581F81A153";
// const char* SERVICE_B_UUID = "45420001-0023-4BD4-BBD5-A6920E4C5653";
// const char* CHARACTERISTIC_B1_UUID = "45420002-0023-4BD4-BBD5-A6920E4C5653";
// const char* CHARACTERISTIC_B2_UUID = "45420003-0023-4BD4-BBD5-A6920E4C5653";

#define SERVICE_B_UUID "0000ffe0-0000-1000-8000-00805f9b34fb"
#define CHARACTERISTIC_B1_UUID "0000ffe1-0000-1000-8000-00805f9b34fa"
#define CHARACTERISTIC_B2_UUID "0000ffe1-0000-1000-8000-00805f9b34fb"
const char* MANUFACTURER_NAME_UUID =
    "2A29";                           // Standard UUID for manufacturer name
const char* SYSTEM_ID_UUID = "2A23";  // Standard UUID for system ID
const char* DEVICE_INFO_SERVICE_UUID = "180A";  // Device Information Service

// #define SERVICE_UUID                "e556ec25-6a2d-436f-a43d-82eab88dcefd"
// #define CONTROL_CHARACTERISTIC_UUID "c4bee434-ae8f-4e67-a741-0607141f185b"
// #define SETTINGS_CHARACTERISTIC_UUID "fe9a02ab-2713-40ef-a677-1716f2c03bad"

NimBLEServer* pServer = NimBLEDevice::getServer();

// Define the global variables
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
NimBLECharacteristic* pCharacteristicB2 =
    nullptr;  // Global pointer to B2 characteristic

/**  None of these are required as they will be handled by the library with
 *defaults. **
 **                       Remove as you see fit for your needs */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        ESP_LOGV("NIMBLE", "Client address: %s",
                 connInfo.getAddress().toString().c_str());

        /**
         *  We can use the connection handle here to ask for different
         * connection parameters. Args: connection handle, min connection
         * interval, max connection interval latency, supervision timeout.
         *  Units; Min/Max Intervals: 1.25 millisecond increments.
         *  Latency: number of intervals allowed to skip.
         *  Timeout: 10 millisecond increments.
         */
        pServer->updateConnParams(connInfo.getConnHandle(), 3, 4, 0, 20);
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                      int reason) override {
        ESP_LOGV("NIMBLE", "Client disconnected - start advertising");
        NimBLEDevice::startAdvertising();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        ESP_LOGV("NIMBLE", "MTU updated: %u for connection ID: %u", MTU,
                 connInfo.getConnHandle());
    }

    /********************* Security handled here *********************/
    uint32_t onPassKeyDisplay() override {
        ESP_LOGV("NIMBLE", "Server Passkey Display");
        /**
         * This should return a random 6 digit number for security
         *  or make your own static passkey as done here.
         */
        return 123456;
    }

    void onConfirmPassKey(NimBLEConnInfo& connInfo,
                          uint32_t pass_key) override {
        ESP_LOGV("NIMBLE", "The passkey YES/NO number: %" PRIu32, pass_key);
        /** Inject false if passkeys don't match. */
        NimBLEDevice::injectConfirmPasskey(connInfo, true);
    }

    void onAuthenticationComplete(NimBLEConnInfo& connInfo) override {
        /** Check that encryption was successful, if not we disconnect the
         * client */
        if (!connInfo.isEncrypted()) {
            NimBLEDevice::getServer()->disconnect(connInfo.getConnHandle());
            ESP_LOGV("NIMBLE",
                     "Encrypt connection failed - disconnecting client");
            return;
        }

        ESP_LOGV("NIMBLE", "Secured connection to: %s",
                 connInfo.getAddress().toString().c_str());
    }
} serverCallbacks;
/** Handler class for characteristic actions */
class CharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    uint32_t lastWriteTime = 0;
    float writeHz = 0;
    const float alpha = 0.1;  // Smoothing factor for exponential moving average

    void onRead(NimBLECharacteristic* pCharacteristic,
                NimBLEConnInfo& connInfo) override {
        ESP_LOGV("NIMBLE", "%s : onRead(), value: %s",
                 pCharacteristic->getUUID().toString().c_str(),
                 pCharacteristic->getValue().c_str());
    }

    void onWrite(NimBLECharacteristic* pCharacteristic,
                 NimBLEConnInfo& connInfo) override {
        std::string value = pCharacteristic->getValue();

        // Print raw bytes for debugging
        std::string hexStr;
        for (unsigned char c : value) {
            char hex[4];
            snprintf(hex, sizeof(hex), "%02X", c);
            hexStr += hex;
            hexStr += " ";
        }
        // ESP_LOGD("NIMBLE", "Received data on %s: [%s]",
        //          pCharacteristic->getUUID().toString().c_str(),
        //          hexStr.c_str());

        // Parse position and time from received bytes
        if (value.length() >= 3) {        // Changed from 4 to 3 bytes
            uint8_t position = value[0];  // First byte is position (0-180)
            uint16_t inTime =
                (value[1] << 8) | value[2];  // Next 2 bytes are time

            // Move to the parsed position
            ossmInterface->moveTo(position * (100.0f / 180.0f), inTime);
        }

        if (value.rfind("startStreaming", 0) == 0) {
            ESP_LOGD("NIMBLE", "Start streaming");
            ossmInterface->ble_click();
            return;
        }

        uint32_t now = millis();
        if (lastWriteTime > 0) {
            float instantHz = 1000.0f / (now - lastWriteTime);
            writeHz = instantHz;
            size_t semicolonPos = value.find(';');
            if (semicolonPos != std::string::npos) {
                std::string points = value;
                size_t pos = 0;

                while ((pos = points.find(';')) != std::string::npos) {
                    std::string posStr = points.substr(0, pos);
                    points.erase(0, pos + 1);

                    uint16_t pos =
                        (uint16_t)strtoul(posStr.c_str(), nullptr, 10);
                    enqueueTarget(
                        pos, 0);  // Set velocity to 0 since we're not using it

                    ESP_LOGV("NIMBLE", "Enqueued point - pos: %u", pos);
                }
            }
        }
        lastWriteTime = now;
    }

    /**
     *  The value returned in code is the NimBLE host return code.
     */
    void onStatus(NimBLECharacteristic* pCharacteristic, int code) override {
        ESP_LOGV("NIMBLE", "Notification/Indication return code: %d, %s", code,
                 NimBLEUtils::returnCodeToString(code));
    }

    /** Peer subscribed to notifications/indications */
    void onSubscribe(NimBLECharacteristic* pCharacteristic,
                     NimBLEConnInfo& connInfo, uint16_t subValue) override {
        std::string str = "Client ID: ";
        str += connInfo.getConnHandle();
        str += " Address: ";
        str += connInfo.getAddress().toString();
        if (subValue == 0) {
            str += " Unsubscribed to ";
        } else if (subValue == 1) {
            str += " Subscribed to notifications for ";
        } else if (subValue == 2) {
            str += " Subscribed to indications for ";
        } else if (subValue == 3) {
            str += " Subscribed to notifications and indications for ";
        }
        str += std::string(pCharacteristic->getUUID());

        ESP_LOGV("NIMBLE", "%s", str.c_str());
    }
} chrCallbacks;

/** Handler class for descriptor actions */
class DescriptorCallbacks : public NimBLEDescriptorCallbacks {
    void onWrite(NimBLEDescriptor* pDescriptor,
                 NimBLEConnInfo& connInfo) override {
        std::string dscVal = pDescriptor->getValue();
        ESP_LOGV("NIMBLE", "Descriptor written value: %s", dscVal.c_str());
    }

    void onRead(NimBLEDescriptor* pDescriptor,
                NimBLEConnInfo& connInfo) override {
        ESP_LOGV("NIMBLE", "%s Descriptor read",
                 pDescriptor->getUUID().toString().c_str());
    }
} dscCallbacks;

void nimbleLoop(void* pvParameters) {
    NimBLEServer* pServer = (NimBLEServer*)pvParameters;
    /** Loop here and send notifications to connected peers */

    vTaskDelete(NULL);
    return;
    while (true) {
        ESP_LOGD("NIMBLE", "Nimble loop");
        vTaskDelay(pdMS_TO_TICKS(2000));

        if (pServer->getConnectedCount()) {
            ESP_LOGD("NIMBLE", "Nimble loop - connected");
            NimBLEService* pSvc = pServer->getServiceByUUID(SERVICE_B_UUID);
            if (pSvc) {
                ESP_LOGD("NIMBLE", "Nimble loop - service found");
                NimBLECharacteristic* pChr =
                    pSvc->getCharacteristic(CHARACTERISTIC_B2_UUID);
                if (pChr) {
                    ESP_LOGD("NIMBLE", "Nimble loop - characteristic found");
                    pChr->notify();
                }
            }
        }
    }
}

void initNimble() {
    NimBLEServer* pServer;
    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("LVS-EB01");

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

    // // Create Service A
    // NimBLEService* pServiceA = pServer->createService(SERVICE_A_UUID);
    // NimBLECharacteristic* pCharA1 = pServiceA->createCharacteristic(
    //     CHARACTERISTIC_A1_UUID, NIMBLE_PROPERTY::READ |
    //     NIMBLE_PROPERTY::WRITE |
    //                                 NIMBLE_PROPERTY::NOTIFY |
    //                                 NIMBLE_PROPERTY::WRITE_NR);
    // NimBLECharacteristic* pCharA2 = pServiceA->createCharacteristic(
    //     CHARACTERISTIC_A2_UUID, NIMBLE_PROPERTY::READ |
    //     NIMBLE_PROPERTY::WRITE |
    //                                 NIMBLE_PROPERTY::NOTIFY |
    //                                 NIMBLE_PROPERTY::WRITE_NR);

    // Create Service B
    NimBLEService* pServiceB = pServer->createService(SERVICE_B_UUID);
    // NimBLECharacteristic* pCharB1 = pServiceB->createCharacteristic(
    //     CHARACTERISTIC_B1_UUID, NIMBLE_PROPERTY::READ |
    //     NIMBLE_PROPERTY::WRITE |
    //                                 NIMBLE_PROPERTY::NOTIFY |
    //                                 NIMBLE_PROPERTY::WRITE_NR);
    NimBLECharacteristic* pCharB2 = pServiceB->createCharacteristic(
        CHARACTERISTIC_B2_UUID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                                    NIMBLE_PROPERTY::NOTIFY |
                                    NIMBLE_PROPERTY::WRITE_NR);

    // Store the characteristic pointer globally
    pCharacteristicB2 = pCharB2;

    // // Set callbacks for all characteristics
    // pCharA1->setCallbacks(&chrCallbacks);
    // pCharA2->setCallbacks(&chrCallbacks);
    // pCharB1->setCallbacks(&chrCallbacks);
    pCharB2->setCallbacks(&chrCallbacks);

    // Start the services
    // pServiceA->start();
    pServiceB->start();

    // Add Device Information Service
    NimBLEService* pDeviceInfoService =
        pServer->createService(DEVICE_INFO_SERVICE_UUID);

    // Add Manufacturer Name characteristic
    NimBLECharacteristic* pManufacturerName =
        pDeviceInfoService->createCharacteristic(MANUFACTURER_NAME_UUID,
                                                 NIMBLE_PROPERTY::READ);
    pManufacturerName->setValue("Silicon Labs");

    // Add System ID characteristic
    NimBLECharacteristic* pSystemId = pDeviceInfoService->createCharacteristic(
        SYSTEM_ID_UUID, NIMBLE_PROPERTY::READ);
    uint8_t systemId[] = {0x88, 0x1A, 0x14, 0xFF, 0xFE, 0x34, 0x29, 0x63};
    pSystemId->setValue(systemId, 8);

    // Start the device info service
    pDeviceInfoService->start();

    // Update advertising to include new services
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("LVS-EB01");
    // pAdvertising->addServiceUUID(pServiceA->getUUID());
    pAdvertising->addServiceUUID(pServiceB->getUUID());
    pAdvertising->addServiceUUID(pDeviceInfoService->getUUID());
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    // Set MTU size for small packets (3 hex numbers)
    NimBLEDevice::setMTU(
        32);  // Using power of 2 MTU size for optimal packet alignment

    xTaskCreatePinnedToCore(
        nimbleLoop, "nimbleLoop", 9 * configMINIMAL_STACK_SIZE, pServer,
        configMAX_PRIORITIES - 1, nullptr, Tasks::stepperCore);
}
