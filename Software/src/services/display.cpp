#include "display.h"

#include <esp_log.h>

SemaphoreHandle_t displayMutex = nullptr;
static auto TAG = "DISPLAY";

static auto ROTATION = U8G2_R0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(ROTATION, Pins::Display::oledReset,
                                            Pins::Remote::displayClock,
                                            Pins::Remote::displayData);

void initDisplay() {
    ESP_LOGI(TAG, "Initializing display...");
    if (displayMutex == nullptr) {
        displayMutex = xSemaphoreCreateMutex();
        if (displayMutex == nullptr) {
            ESP_LOGE(TAG, "Failed to create display mutex");
        }
    }
    display.begin();
    display.setI2CAddress(0x3C << 1);  // 0x3C is common for 128x64 OLEDs
    display.setPowerSave(0);
    display.setContrast(255);
    display.clearBuffer();
    display.sendBuffer();

    ESP_LOGI(TAG, "Display initialization complete.");

#ifdef LOG_SCREEN_STATE
    startScreenLogTask();
#endif
}

#define ICON_TILES 4

void clearIcons() {
    display.setClipWindow(128 - 8 * ICON_TILES, 0, 128, 8);
    display.clearBuffer();
}

void refreshIcons() {
    display.setMaxClipWindow();
    display.updateDisplayArea(16 - ICON_TILES, 0, ICON_TILES, 1);
}

void refreshHeader() {
    display.setMaxClipWindow();
    display.updateDisplayArea(0, 0, 16 - ICON_TILES, 1);
}

void refreshPage(bool includeFooter, bool includeHeader) {
    display.setMaxClipWindow();

    if (includeFooter) {
        refreshFooter();
    }
    if (includeHeader) {
        refreshHeader();
    }

    display.updateDisplayArea(0, 1, 16, 6);
}

void refreshFooter() {
    display.setMaxClipWindow();
    display.updateDisplayArea(0, 7, 16, 1);
}
