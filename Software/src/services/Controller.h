#ifndef SOFTWARE_CONTROLLER_H
#define SOFTWARE_CONTROLLER_H

// Reference: https://github.com/h2zero/NimBLE-Arduino/blob/master/examples/NimBLE_Client/NimBLE_Client.ino

#include <Arduino.h>

#include "NimBLEDevice.h"
#include "XboxControllerNotificationParser.h"

XboxControllerNotificationParser xboxNotif;

void scanEndedCB(NimBLEScanResults results);

static NimBLEAdvertisedDevice* advDevice;

bool scanning = false;
bool connected = false;
static uint32_t scanTime = 0; /** 0 = scan forever */

static NimBLEAddress targetDeviceAddress("0C:35:26:EB:39:3A");

static NimBLEUUID uuidServiceUnknown("00000001-5f60-4c4f-9c83-a7953298d40d");
static NimBLEUUID uuidServiceGeneral("1801");
static NimBLEUUID uuidServiceBattery("180f");
static NimBLEUUID uuidServiceHid("1812");
static NimBLEUUID uuidCharaReport("2a4d");
static NimBLEUUID uuidCharaPnp("2a50");
static NimBLEUUID uuidCharaHidInformation("2a4a");
static NimBLEUUID uuidCharaPeripheralAppearance("2a01");
static NimBLEUUID uuidCharaPeripheralControlParameters("2a04");

class ClientCallbacks : public NimBLEClientCallbacks
{
    void onConnect(NimBLEClient* pClient)
    {
        Serial.println("Connected");
        connected = true;
        // pClient->updateConnParams(120,120,0,60);
    };

    void onDisconnect(NimBLEClient* pClient)
    {
        Serial.print(pClient->getPeerAddress().toString().c_str());
        Serial.println(" Disconnected");
        connected = false;
    };

    /** Called when the peripheral requests a change to the connection parameters.
     *  Return true to accept and apply them or false to reject and keep
     *  the currently used parameters. Default will return true.
     */
    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params)
    {
        Serial.print("onConnParamsUpdateRequest");
        if (params->itvl_min < 24)
        { /** 1.25ms units */
            return false;
        }
        else if (params->itvl_max > 40)
        { /** 1.25ms units */
            return false;
        }
        else if (params->latency > 2)
        { /** Number of intervals allowed to skip */
            return false;
        }
        else if (params->supervision_timeout > 100)
        { /** 10ms units */
            return false;
        }

        return true;
    };

    /********************* Security handled here **********************
     ****** Note: these are the same return values as defaults ********/
    uint32_t onPassKeyRequest()
    {
        Serial.println("Client Passkey Request");
        /** return the passkey to send to the server */
        return 0;
    };

    bool onConfirmPIN(uint32_t pass_key)
    {
        Serial.print("The passkey YES/NO number: ");
        Serial.println(pass_key);
        /** Return false if passkeys don't match. */
        return true;
    };

    /** Pairing process complete, we can check the results in ble_gap_conn_desc */
    void onAuthenticationComplete(ble_gap_conn_desc* desc)
    {
        Serial.println("onAuthenticationComplete");
        if (!desc->sec_state.encrypted)
        {
            Serial.println("Encrypt connection failed - disconnecting");
            /** Find the client with the connection handle provided in desc */
            NimBLEDevice::getClientByID(desc->conn_handle)->disconnect();
            return;
        }
    };
};

/** Define a class to handle the callbacks when advertisments are received */
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks
{
    void onResult(NimBLEAdvertisedDevice* advertisedDevice)
    {
        Serial.print("Advertised Device found: ");
        Serial.println(advertisedDevice->toString().c_str());
        Serial.printf("name:%s, address:%s\n", advertisedDevice->getName().c_str(),
                      advertisedDevice->getAddress().toString().c_str());
        Serial.printf("uuidService:%s\n", advertisedDevice->haveServiceUUID()
                                              ? advertisedDevice->getServiceUUID().toString().c_str()
                                              : "none");

        if (advertisedDevice->getAddress().equals(targetDeviceAddress))
        // if (advertisedDevice->isAdvertisingService(uuidServiceHid))
        {
            Serial.println("Found Our Service");
            /** stop scan before connecting */
            NimBLEDevice::getScan()->stop();
            /** Save the device reference in a global for the client to use*/
            advDevice = advertisedDevice;
        }
    };
};

unsigned long printInterval = 100UL;

/** Notification / Indication receiving handler callback */
void notifyCB(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify)
{
    static bool isPrinting = false;
    static unsigned long printedAt = 0;
    if (isPrinting || millis() - printedAt < printInterval) return;
    isPrinting = true;
    std::string str = (isNotify == true) ? "Notification" : "Indication";
    str += " from ";
    /** NimBLEAddress and NimBLEUUID have std::string operators */
    str += std::string(pRemoteCharacteristic->getRemoteService()->getClient()->getPeerAddress());
    str += ": Service = " + std::string(pRemoteCharacteristic->getRemoteService()->getUUID());
    str += ", Characteristic = " + std::string(pRemoteCharacteristic->getUUID());
    // str += ", Value = " + std::string((char*)pData, length);
    Serial.println(str.c_str());
    Serial.print("value: ");
    for (int i = 0; i < length; ++i)
    {
        Serial.printf(" %02x", pData[i]);
    }
    Serial.println("");
    xboxNotif.update(pData, length);
    Serial.print(xboxNotif.toString());
    printedAt = millis();
    isPrinting = false;
}

void scanEndedCB(NimBLEScanResults results)
{
    Serial.println("Scan Ended");
    scanning = false;
    // sprint results names
    for (int i = 0; i < results.getCount(); ++i)
    {
        Serial.println(results.getDevice(i)->getName().c_str());
    }
}

static ClientCallbacks clientCB;

void charaPrintId(NimBLERemoteCharacteristic* pChara)
{
    Serial.printf("s:%s c:%s h:%d", pChara->getRemoteService()->getUUID().toString().c_str(),
                  pChara->getUUID().toString().c_str(), pChara->getHandle());
}

void printValue(std::__cxx11::string str)
{
    Serial.printf("str: %s\n", str.c_str());
    Serial.printf("hex:");
    for (auto v : str)
    {
        Serial.printf(" %02x", v);
    }
    Serial.println("");
}

void charaRead(NimBLERemoteCharacteristic* pChara)
{
    if (pChara->canRead())
    {
        charaPrintId(pChara);
        Serial.println(" canRead");
        auto str = pChara->readValue();
        if (str.size() == 0)
        {
            str = pChara->readValue();
        }
        printValue(str);
    }
}

void charaSubscribeNotification(NimBLERemoteCharacteristic* pChara)
{
    if (pChara->canNotify())
    {
        charaPrintId(pChara);
        Serial.println(" canNotify ");
        if (pChara->subscribe(true, notifyCB, true))
        {
            Serial.println("set notifyCb");
            // return true;
        }
        else
        {
            Serial.println("failed to subscribe");
        }
    }
}

bool afterConnect(NimBLEClient* pClient)
{
    for (auto pService : *pClient->getServices(true))
    {
        auto sUuid = pService->getUUID();
        if (!sUuid.equals(uuidServiceHid))
        {
            continue; // skip
        }
        Serial.println(pService->toString().c_str());
        for (auto pChara : *pService->getCharacteristics(true))
        {
            charaRead(pChara);
            charaSubscribeNotification(pChara);
        }
    }

    return true;
}

/** Handles the provisioning of clients and connects / interfaces with the
 * server */
bool connectToServer(NimBLEAdvertisedDevice* advDevice)
{
    NimBLEClient* pClient = nullptr;

    /** Check if we have a client we should reuse first **/
    if (NimBLEDevice::getClientListSize())
    {
        pClient = NimBLEDevice::getClientByPeerAddress(advDevice->getAddress());
        if (pClient)
        {
            pClient->connect();
        }
    }

    /** No client to reuse? Create a new one. */
    if (!pClient)
    {
        if (NimBLEDevice::getClientListSize() >= NIMBLE_MAX_CONNECTIONS)
        {
            Serial.println("Max clients reached - no more connections available");
            return false;
        }

        pClient = NimBLEDevice::createClient();

        Serial.println("New client created");

        pClient->setClientCallbacks(&clientCB, false);
        pClient->setConnectionParams(12, 12, 0, 51);
        pClient->setConnectTimeout(5);
        pClient->connect(advDevice, false);
    }

    int retryCount = 5;
    while (!pClient->isConnected())
    {
        if (retryCount <= 0)
        {
            return false;
        }
        else
        {
            Serial.println("try connection again " + String(millis()));
            delay(1000);
        }

        NimBLEDevice::getScan()->stop();
        pClient->disconnect();
        delay(500);
        // Serial.println(pClient->toString().c_str());
        pClient->connect(true);
        --retryCount;
    }

    Serial.print("Connected to: ");
    Serial.println(pClient->getPeerAddress().toString().c_str());
    Serial.print("RSSI: ");
    Serial.println(pClient->getRssi());

    pClient->discoverAttributes();

    bool result = afterConnect(pClient);
    if (!result)
    {
        return result;
    }

    Serial.println("Done with this device!");
    return true;
}

void startScan()
{
    scanning = true;
    auto pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    pScan->setInterval(45);
    pScan->setWindow(15);
    Serial.println("Start scan");
    pScan->start(scanTime, scanEndedCB);
}

#endif // SOFTWARE_CONTROLLER_H
