#include "nimble.h"

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

                    uint16_t pos = (uint16_t)strtoul(posStr.c_str(), nullptr, 10);
                    enqueueTarget(pos, 0); // Set velocity to 0 since we're not using it

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

void initNimble() {
    NimBLEServer* pServer;
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

    NimBLEService* pDeadService = pServer->createService("DEAD");
    NimBLECharacteristic* pBeefCharacteristic =
        pDeadService->createCharacteristic(
            "BEEF",
            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                /** Require a secure connection for read and write access */
                NIMBLE_PROPERTY::READ_ENC |  // only allow reading if paired /
                                             // encrypted
                NIMBLE_PROPERTY::WRITE_ENC   // only allow writing if paired /
                                             // encrypted
        );

    pBeefCharacteristic->setValue("Burger");
    pBeefCharacteristic->setCallbacks(&chrCallbacks);

    /**
     *  2902 and 2904 descriptors are a special case, when createDescriptor is
     * called with either of those uuid's it will create the associated class
     * with the correct properties and sizes. However we must cast the returned
     * reference to the correct type as the method only returns a pointer to the
     * base NimBLEDescriptor class.
     */
    NimBLE2904* pBeef2904 = pBeefCharacteristic->create2904();
    pBeef2904->setFormat(NimBLE2904::FORMAT_UTF8);
    pBeef2904->setCallbacks(&dscCallbacks);

    NimBLEService* pBaadService = pServer->createService("BAAD");
    NimBLECharacteristic* pFoodCharacteristic =
        pBaadService->createCharacteristic(
            "F00D", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
                        NIMBLE_PROPERTY::NOTIFY | NIMBLE_PROPERTY::WRITE_NR);

    pFoodCharacteristic->setValue("Fries");
    pFoodCharacteristic->setCallbacks(&chrCallbacks);

    /** Custom descriptor: Arguments are UUID, Properties, max length of the
     * value in bytes */
    NimBLEDescriptor* pC01Ddsc = pFoodCharacteristic->createDescriptor(
        "C01D",
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE |
            NIMBLE_PROPERTY::WRITE_ENC,
        20);
    pC01Ddsc->setValue("Send it back!");
    pC01Ddsc->setCallbacks(&dscCallbacks);

    /** Start the services when finished creating all Characteristics and
     * Descriptors */
    pDeadService->start();
    pBaadService->start();

    /** Create an advertising instance and add the services to the advertised
     * data */
    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setName("NimBLE-Server");
    pAdvertising->addServiceUUID(pDeadService->getUUID());
    pAdvertising->addServiceUUID(pBaadService->getUUID());
    /**
     *  If your device is battery powered you may consider setting scan response
     *  to false as it will extend battery life at the expense of less data
     * sent.
     */
    pAdvertising->enableScanResponse(true);
    pAdvertising->start();

    // Increase MTU size for larger packets
    NimBLEDevice::setMTU(517);  // Maximum MTU size
}
