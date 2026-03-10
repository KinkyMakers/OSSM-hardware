#include "test_helpers.h"

#include <cstdio>

struct BleEntry {
    ui::BleStatus status;
    const char* name;
};

struct WifiEntry {
    ui::WifiStatus status;
    const char* name;
};

static const BleEntry bleStates[] = {
    {ui::BleStatus::DISCONNECTED, "ble_disconnected"},
    {ui::BleStatus::CONNECTING, "ble_connecting"},
    {ui::BleStatus::CONNECTED, "ble_connected"},
    {ui::BleStatus::ADVERTISING, "ble_advertising"},
    {ui::BleStatus::ERROR, "ble_error"},
};
static const int NUM_BLE = sizeof(bleStates) / sizeof(bleStates[0]);

static const WifiEntry wifiStates[] = {
    {ui::WifiStatus::DISCONNECTED, "wifi_disconnected"},
    {ui::WifiStatus::CONNECTING, "wifi_connecting"},
    {ui::WifiStatus::CONNECTED, "wifi_connected"},
    {ui::WifiStatus::ERROR, "wifi_error"},
};
static const int NUM_WIFI = sizeof(wifiStates) / sizeof(wifiStates[0]);

void test_drawHeaderIcons_allCombinations(void) {
    int combo = 0;
    for (int w = 0; w < NUM_WIFI; w++) {
        for (int b = 0; b < NUM_BLE; b++) {
            initTestDisplay(&u8g2);

            ui::HeaderIconsData data{
                wifiStates[w].status,
                bleStates[b].status,
            };

            ui::drawHeaderIcons(&u8g2, data);
            u8g2_SetMaxClipWindow(&u8g2);

            TEST_ASSERT_TRUE(bufferHasContent(&u8g2));

            char name[128];
            snprintf(name, sizeof(name), "%02d_%s_%s", combo,
                     wifiStates[w].name, bleStates[b].name);
            TEST_ASSERT_TRUE(
                savePBMGrouped(&u8g2, "header_icons", name));
            combo++;
        }
    }
    printf("  -> Generated %d header icon combinations\n", combo);
}

void register_header_icons_tests() {
    RUN_TEST(test_drawHeaderIcons_allCombinations);
}
