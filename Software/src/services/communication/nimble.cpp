#include "nimble.h"

#include <ArduinoJson.h>
#include <constants/LogTags.h>
#include <constants/UserConfig.h>
#include <services/board.h>
#include <services/tasks.h>

#include <queue>

#include "command.hpp"
#include "command/commands.hpp"
#include "config.hpp"
#include "patterns.hpp"
#include "state.hpp"

// Define the global variables
NimBLEServer* pServer = nullptr;

NimBLECharacteristic* pStateCharacteristic = nullptr;

NimBLECharacteristic* pSpeedKnobConfigCharacteristic = nullptr;

NimBLECharacteristic* pCommandCharacteristic = nullptr;

static long lostConnectionTime = 0;
static int speedOnLostConnection = 0;
static const unsigned long RAMP_DURATION_MS =
    2000;  // Duration for speed ramp to zero

double easeInOutSine(double t) {
    return 0.5 * (1 + sin(3.1415926 * (t - 0.5)));
}

/** Handler class for server actions */
class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo) override {
        ESP_LOGI(NIMBLE_TAG, "Client connected: %s",
                 connInfo.getAddress().toString().c_str());
        ESP_LOGI(NIMBLE_TAG, "Connection count: %d",
                 pServer->getConnectedCount());

        // Set BLE connection status to true
        if (ossmInterface) {
            ossmInterface->setBLEConnectionStatus(true);
        }

        lostConnectionTime = 0;
    }

    void onDisconnect(NimBLEServer* pServer, NimBLEConnInfo& connInfo,
                      int reason) override {
        ESP_LOGI(NIMBLE_TAG, "Client disconnected: %s, reason: %d",
                 connInfo.getAddress().toString().c_str(), reason);
        ESP_LOGI(NIMBLE_TAG, "Connection count: %d",
                 pServer->getConnectedCount());

        // Set BLE connection status to false when no connections remain
        if (ossmInterface && pServer->getConnectedCount() == 0) {
            ossmInterface->setBLEConnectionStatus(false);
        }

        // Capture current speed when connection is lost
        speedOnLostConnection = ossmInterface->getSpeed();
        ESP_LOGI(NIMBLE_TAG, "Speed on disconnect: %d", speedOnLostConnection);

        // Restart advertising when client disconnects
        if (pServer->getConnectedCount() == 0) {
            ESP_LOGI(NIMBLE_TAG,
                     "No connections remaining, restarting advertising");
            pServer->startAdvertising();
        }

        lostConnectionTime = millis();
    }

    void onMTUChange(uint16_t MTU, NimBLEConnInfo& connInfo) override {
        ESP_LOGD(NIMBLE_TAG, "MTU changed to: %d for connection: %s", MTU,
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
                ESP_LOGI(NIMBLE_TAG,
                         "No connections and not advertising, restarting "
                         "advertising");
                pServer->startAdvertising();
            }

            if (lostConnectionTime > 0) {
                // Skip ramp-down if speed was already zero when connection was
                // lost
                if (speedOnLostConnection <= 0) {
                    lostConnectionTime = 0;
                    continue;
                }

                unsigned long elapsed = millis() - lostConnectionTime;

                // Wait 1 second before starting easing
                if (elapsed < 1000) {
                    vTaskDelay(pdMS_TO_TICKS(50));
                    continue;
                }

                if (elapsed > 1000 + RAMP_DURATION_MS) {
                    ESP_LOGI(
                        NIMBLE_TAG,
                        "Speed ramp duration exceeded, setting speed to 0");
                    lostConnectionTime = 0;
                    speedOnLostConnection = 0;
                    ossmInterface->ble_click("set:speed:0");
                    continue;
                }

                // Calculate easing factor (0 to 1) over ramp duration
                double progress = constrain(
                    (elapsed - 1000) / (double)RAMP_DURATION_MS, 0.0, 1.0);
                double t = easeInOutSine(progress);

                // Ramp from current speed to zero
                int targetSpeed = (int)(speedOnLostConnection * (1.0 - t));
                ESP_LOGI(NIMBLE_TAG,
                         "Target speed: %d (from %d, progress: %.2f)",
                         targetSpeed, speedOnLostConnection, progress);

                ossmInterface->ble_click("set:speed:" + String(targetSpeed));

                // Stop processing when easing is complete
                if (t >= 1) {
                    lostConnectionTime = 0;
                    continue;
                }

                vTaskDelay(pdMS_TO_TICKS(50));
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(200));
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

        // mannage message queue
        while (!messageQueue.empty()) {
            String cmd = messageQueue.front();
            messageQueue.pop();
            ossmInterface->ble_click(cmd);
            pChr->setValue("ok:" + cmd);
            vTaskDelay(1);
        }

        int currentTime = millis();
        bool stateChanged = currentState != lastState;
        bool timeElapsed = (currentTime - lastMessageTime) > 1000;

        if (!stateChanged && !timeElapsed) {
            vTaskDelay(1);
            continue;
        }
        lastMessageTime = currentTime;

        if (stateChanged) {
            ESP_LOGD(NIMBLE_TAG, "State changed to: %s", currentState.c_str());
        }
        pChr->setValue(currentState);
        pChr->notify();
        lastState = currentState;
        vTaskDelay(1);
    }
}

void initNimble() {
    /** Initialize NimBLE and set the device name */
    NimBLEDevice::init("OSSM");

    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_SC);
    pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(&serverCallbacks);

    // Create Service
    NimBLEService* pService = pServer->createService(SERVICE_UUID);

    pCommandCharacteristic =
        initCommandCharacteristic(pService, NimBLEUUID(CHARACTERISTIC_UUID));

    pSpeedKnobConfigCharacteristic = initSpeedKnobConfigCharacteristic(
        pService, NimBLEUUID(CHARACTERISTIC_SPEED_KNOB_CONFIG_UUID));

    pStateCharacteristic = initStateCharacteristic(
        pService, NimBLEUUID(CHARACTERISTIC_STATE_UUID));

    initPatternsCharacteristic(pService,
                               NimBLEUUID(CHARACTERISTIC_PATTERNS_UUID));
    initPatternDataCharacteristic(
        pService, NimBLEUUID(CHARACTERISTIC_GET_PATTERN_DATA_UUID));

    // Start the services
    pService->start();

    // Add Device Information Service
    NimBLEService* pDeviceInfoService =
        pServer->createService(DEVICE_INFO_SERVICE_UUID);

    // Add Manufacturer Name characteristic
    NimBLECharacteristic* pManufacturerName =
        pDeviceInfoService->createCharacteristic(MANUFACTURER_NAME_UUID,
                                                 NIMBLE_PROPERTY::READ);
    static const char manufacturer[] PROGMEM = "Research And Desire";
    pManufacturerName->setValue(String(FPSTR(manufacturer)));

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
        nimbleLoop, "nimbleLoop", 5 * configMINIMAL_STACK_SIZE, pServer,
        configMAX_PRIORITIES - 1, nullptr, Tasks::stepperCore);
}
