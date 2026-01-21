#include "display.h"

#include <esp_log.h>

SemaphoreHandle_t displayMutex = nullptr;
static auto TAG = "DISPLAY";

#ifdef LOG_SCREEN_STATE
static TaskHandle_t screenLogTaskHandle = nullptr;

// Chunk size for serial output (avoid ESP_LOG truncation)
static constexpr size_t BYTES_PER_LINE = 128;  // One tile row at a time

static void screenLogTask(void* pvParameters) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(62));  // ~16 fps

        if (xSemaphoreTake(displayMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            display.setClipWindow(0, 0, 128, 64);
            uint8_t* buffer = display.getBufferPtr();

            // Get actual buffer dimensions from U8g2
            const size_t tileWidth =
                display
                    .getBufferTileWidth();  // tiles horizontally (16 for 128px)
            const size_t tileHeight =
                display.getBufferTileHeight();  // tiles vertically (8 for 64px)
            const size_t rowBytes = tileWidth * 8;  // bytes per tile row
            const size_t totalBytes =
                rowBytes * tileHeight;  // total buffer size

            // Frame start marker with dimensions
            printf("FRAME_START:%dx%d:%zu\n", display.getDisplayWidth(),
                   display.getDisplayHeight(), totalBytes);

            // Output buffer in tile rows to avoid log truncation
            // Buffer hex for one row: 128 bytes * 2 chars + null
            static char hexBuffer[BYTES_PER_LINE * 2 + 1];

            for (size_t row = 0; row < tileHeight; row++) {
                const size_t offset = row * rowBytes;

                // Convert this row to hex
                for (size_t i = 0; i < rowBytes; i++) {
                    sprintf(&hexBuffer[i * 2], "%02X", buffer[offset + i]);
                }
                hexBuffer[rowBytes * 2] = '\0';

                // Output row with index
                printf("ROW%zu:%s\n", row, hexBuffer);
            }

            printf("FRAME_END\n");
            fflush(stdout);

            xSemaphoreGive(displayMutex);
        }
    }
}

static void startScreenLogTask() {
    xTaskCreatePinnedToCore(screenLogTask, "screenLog", 4096, nullptr,
                            1,  // Low priority
                            &screenLogTaskHandle,
                            0  // Core 0
    );
    ESP_LOGI(TAG, "Screen logging task started");
}
#endif

static auto ROTATION = U8G2_R0;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(ROTATION, Pins::Display::oledReset,
                                            Pins::Remote::displayClock,
                                            Pins::Remote::displayData);

const char EMPTY_STRING[] PROGMEM = "";

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

void clearHeader() {
    display.setClipWindow(0, 0, 128 - 8 * ICON_TILES, 8);
    display.clearBuffer();
}

void clearFooter() {
    display.setClipWindow(0, 64, 128, 64);
    display.clearBuffer();
}

auto clearPage(const bool includeFooter, const bool includeHeader) -> void {
    const int y1 = includeHeader ? 0 : 8;
    const int y2 = includeFooter ? 64 : 56;

    display.setClipWindow(0, y1, 128, y2);
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

int drawWrappedText(const int x, const int y, const String& text,
                    const bool center) {
    const int maxWidth = display.getDisplayWidth() - x;
    const int lineHeight = display.getMaxCharHeight();
    const int fontHeight = display.getFontAscent();
    int cursorY = y + fontHeight;
    int lineCount = 0;

    String word;
    String line;
    const size_t textLen = text.length();

    auto drawLine = [&](const String& l) {
        int drawX = x;
        if (center) {
            const int lineWidth = display.getStrWidth(l.c_str());
            drawX = (display.getDisplayWidth() - x - lineWidth) / 2 + x;
        }
        display.drawStr(drawX, cursorY, l.c_str());
        cursorY += lineHeight;
        lineCount++;
    };

    for (size_t i = 0; i < textLen; ++i) {
        // Handle literal "\\n" as a line break
        if (text[i] == '\\' && i + 1 < textLen && text[i + 1] == 'n') {
            if (word.length() > 0) {
                if (line.length() > 0) {
                    line += ' ';
                }
                line += word;
            }
            drawLine(line);
            line.clear();
            word.clear();
            i++;  // Skip the 'n'
            continue;
        }
        char c = text[i];
        if (c == ' ' || c == '\n' || i == textLen - 1) {
            if (i == textLen - 1 && c != ' ' && c != '\n') {
                word += c;
            }
            // Prepare testLine = line + (line empty? "" : " ") + word
            String testLine = line;
            if (testLine.length() > 0) {
                testLine += ' ';
            }
            testLine += word;
            if (const int testWidth = display.getStrWidth(testLine.c_str());
                testWidth > maxWidth && line.length() > 0) {
                drawLine(line);
                line = word;
            } else {
                if (line.length() > 0) {
                    line += ' ';
                }
                line += word;
            }
            word.clear();
            if (c == '\n') {
                drawLine(line);
                line.clear();
            }
        } else {
            word += c;
        }
    }
    if (line.length() > 0) {
        drawLine(line);
    }
    return lineCount;
}

static String lastHeaderText = EMPTY_STRING;
static String lastFooterLeftText = EMPTY_STRING;
static String lastFooterRightText = EMPTY_STRING;

void setHeader(String& text) {
    if (text == lastHeaderText) {
        return;
    }
    lastHeaderText = text;
    text.toUpperCase();
    clearHeader();
    display.setFont(u8g2_font_spleen5x8_mu);
    display.drawStr(0, 8, text.c_str());
    refreshHeader();
}

void setFooter(String& left, String& right) {
    if (left == lastFooterLeftText && right == lastFooterRightText) {
        return;
    }
    lastFooterLeftText = left;
    lastFooterRightText = right;
    left.toUpperCase();
    right.toUpperCase();
    clearFooter();
    display.setFont(u8g2_font_spleen5x8_mu);
    display.drawStr(0, 64, left.c_str());
    display.drawStr(128 - display.getStrWidth(right.c_str()) - 8, 64,
                    right.c_str());
    refreshFooter();
}
