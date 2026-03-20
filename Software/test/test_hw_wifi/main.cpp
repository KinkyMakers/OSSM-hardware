/**
 * Hardware WiFi Tests — require USB-connected OSSM
 *
 * Run:  pio test -e development -f test_hw_wifi
 *
 * These tests verify WiFi credential storage (NVS), the WiFi config BLE
 * characteristic, and the WiFi status JSON output on real hardware.
 * No motor motion is commanded.
 *
 * NOTE: These tests write temporary credentials to NVS for validation.
 *       The teardown restores any previously saved credentials.
 */

#include <Arduino.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <WiFi.h>
#include <unity.h>

#include "constants/Config.h"
#include "services/communication/nimble.h"
#include "services/communication/wifi.hpp"
#include "services/wm.h"

// ─── Globals ───────────────────────────────────────────────

static NimBLEServer* testServer = nullptr;
static NimBLEService* testService = nullptr;
static NimBLECharacteristic* wifiChar = nullptr;

// Saved credentials to restore after test
static String savedSSID;
static String savedPassword;
static bool hadSavedCredentials = false;

// ─── Helpers ───────────────────────────────────────────────

static void backupCredentials() {
    Preferences prefs;
    if (prefs.begin("wifi", true)) {
        savedSSID = prefs.getString("ssid", "");
        savedPassword = prefs.getString("password", "");
        hadSavedCredentials = savedSSID.length() > 0;
        prefs.end();
    }
}

static void restoreCredentials() {
    if (hadSavedCredentials) {
        setWiFiCredentials(savedSSID, savedPassword);
    } else {
        // Clear test credentials
        Preferences prefs;
        if (prefs.begin("wifi", false)) {
            prefs.remove("ssid");
            prefs.remove("password");
            prefs.end();
        }
    }
}

void setUp(void) {}
void tearDown(void) {}

// ─── WiFi Init Tests ───────────────────────────────────────

/**
 * WiFi module initialises without crash (station mode).
 */
void test_wifi_initialises(void) {
    TEST_ASSERT_TRUE_MESSAGE(
        WiFi.mode(WIFI_STA),
        "Failed to set WiFi to station mode");
}

// ─── NVS Credential Storage Tests ──────────────────────────

/**
 * setWiFiCredentials should return true and persist to NVS.
 */
void test_set_wifi_credentials_saves_to_nvs(void) {
    bool result = setWiFiCredentials("TestSSID_HW", "TestPass123");
    TEST_ASSERT_TRUE_MESSAGE(result, "setWiFiCredentials should return true");

    // Verify by reading NVS directly
    Preferences prefs;
    TEST_ASSERT_TRUE_MESSAGE(
        prefs.begin("wifi", true),
        "Failed to open NVS for verification");

    String ssid = prefs.getString("ssid", "");
    String password = prefs.getString("password", "");
    prefs.end();

    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "TestSSID_HW", ssid.c_str(),
        "SSID not saved correctly to NVS");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "TestPass123", password.c_str(),
        "Password not saved correctly to NVS");
}

/**
 * Overwriting credentials should update NVS with new values.
 */
void test_overwrite_credentials(void) {
    setWiFiCredentials("FirstSSID", "FirstPass1");
    setWiFiCredentials("SecondSSID", "SecondPass2");

    Preferences prefs;
    prefs.begin("wifi", true);
    String ssid = prefs.getString("ssid", "");
    prefs.end();

    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "SecondSSID", ssid.c_str(),
        "Overwritten SSID should be the latest value");
}

/**
 * Credentials with special characters should round-trip through NVS.
 */
void test_credentials_special_chars(void) {
    String testSSID = "My WiFi!@#";
    String testPass = "p@$$w0rd;|&<>";

    bool result = setWiFiCredentials(testSSID, testPass);
    TEST_ASSERT_TRUE(result);

    Preferences prefs;
    prefs.begin("wifi", true);
    String readSSID = prefs.getString("ssid", "");
    String readPass = prefs.getString("password", "");
    prefs.end();

    TEST_ASSERT_EQUAL_STRING(testSSID.c_str(), readSSID.c_str());
    TEST_ASSERT_EQUAL_STRING(testPass.c_str(), readPass.c_str());
}

// ─── WiFi Status JSON Tests ────────────────────────────────

/**
 * getWiFiStatus should return valid JSON with expected keys.
 */
void test_wifi_status_json_structure(void) {
    String status = getWiFiStatus();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, status);
    TEST_ASSERT_TRUE_MESSAGE(
        err == DeserializationError::Ok,
        "getWiFiStatus should return valid JSON");

    TEST_ASSERT_TRUE_MESSAGE(
        doc.containsKey("connected"),
        "Status JSON must contain 'connected' key");
    TEST_ASSERT_TRUE_MESSAGE(
        doc.containsKey("ip"),
        "Status JSON must contain 'ip' key");
    TEST_ASSERT_TRUE_MESSAGE(
        doc.containsKey("rssi"),
        "Status JSON must contain 'rssi' key");
}

/**
 * When disconnected, status should report connected=false and empty IP.
 */
void test_wifi_status_when_disconnected(void) {
    // Ensure we're disconnected for this test
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
        delay(500);
    }

    String status = getWiFiStatus();
    JsonDocument doc;
    deserializeJson(doc, status);

    TEST_ASSERT_FALSE_MESSAGE(
        doc["connected"].as<bool>(),
        "Should report disconnected when WiFi is off");
    TEST_ASSERT_EQUAL_STRING_MESSAGE(
        "", doc["ip"].as<const char*>(),
        "IP should be empty when disconnected");
    TEST_ASSERT_EQUAL_MESSAGE(
        0, doc["rssi"].as<int>(),
        "RSSI should be 0 when disconnected");
}

// ─── BLE WiFi Config Characteristic Tests ──────────────────

/**
 * The WiFi config BLE characteristic should be created successfully.
 */
void test_wifi_characteristic_created(void) {
    TEST_ASSERT_NOT_NULL_MESSAGE(
        wifiChar, "WiFi config characteristic creation failed");
}

/**
 * The WiFi config characteristic should support READ and WRITE.
 */
void test_wifi_characteristic_properties(void) {
    uint16_t props = wifiChar->getProperties();

    TEST_ASSERT_TRUE_MESSAGE(
        props & NIMBLE_PROPERTY::READ,
        "WiFi config characteristic must support READ");
    TEST_ASSERT_TRUE_MESSAGE(
        props & NIMBLE_PROPERTY::WRITE,
        "WiFi config characteristic must support WRITE");
}

/**
 * The WiFi config characteristic UUID should match.
 */
void test_wifi_characteristic_uuid(void) {
    NimBLEUUID expectedUUID(CHARACTERISTIC_WIFI_CONFIG_UUID);
    NimBLEUUID actualUUID = wifiChar->getUUID();

    TEST_ASSERT_TRUE_MESSAGE(
        actualUUID == expectedUUID,
        "WiFi config characteristic UUID mismatch");
}

/**
 * The WiFi config characteristic initial value should be valid JSON
 * (set to WiFi status on init).
 */
void test_wifi_characteristic_initial_value_is_json(void) {
    std::string raw = wifiChar->getValue();
    String value = String(raw.c_str());

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, value);
    TEST_ASSERT_TRUE_MESSAGE(
        err == DeserializationError::Ok,
        "WiFi config characteristic initial value should be valid JSON");
}

// ─── connectWiFi with bad credentials ──────────────────────

/**
 * connectWiFi with a bogus SSID should return false (no crash).
 */
void test_connect_wifi_bad_credentials_returns_false(void) {
    setWiFiCredentials("NONEXISTENT_NETWORK_12345", "badpass99");
    bool result = connectWiFi();
    TEST_ASSERT_FALSE_MESSAGE(
        result,
        "connectWiFi should return false for nonexistent network");
}

// ─── Runner ─────────────────────────────────────────────────

void setup() {
    delay(2000);  // give serial monitor time to connect

    // Backup any existing WiFi credentials before tests
    backupCredentials();

    // Initialise WiFi
    WiFi.mode(WIFI_STA);
    initWM();
    delay(500);

    // Initialise NimBLE for characteristic tests
    NimBLEDevice::init("OSSM");
    NimBLEDevice::setSecurityAuth(BLE_SM_PAIR_AUTHREQ_SC);
    testServer = NimBLEDevice::createServer();
    testService = testServer->createService(SERVICE_UUID);

    wifiChar = initWiFiConfigCharacteristic(
        testService, NimBLEUUID(CHARACTERISTIC_WIFI_CONFIG_UUID));

    testService->start();

    UNITY_BEGIN();

    // WiFi init
    RUN_TEST(test_wifi_initialises);

    // NVS credential storage
    RUN_TEST(test_set_wifi_credentials_saves_to_nvs);
    RUN_TEST(test_overwrite_credentials);
    RUN_TEST(test_credentials_special_chars);

    // WiFi status JSON
    RUN_TEST(test_wifi_status_json_structure);
    RUN_TEST(test_wifi_status_when_disconnected);

    // BLE WiFi config characteristic
    RUN_TEST(test_wifi_characteristic_created);
    RUN_TEST(test_wifi_characteristic_properties);
    RUN_TEST(test_wifi_characteristic_uuid);
    RUN_TEST(test_wifi_characteristic_initial_value_is_json);

    // Connection attempt (runs last — has 10s timeout)
    RUN_TEST(test_connect_wifi_bad_credentials_returns_false);

    UNITY_END();

    // Restore original credentials
    restoreCredentials();
}

void loop() {}
