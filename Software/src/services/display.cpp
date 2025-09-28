#include "display.h"

#include <esp_log.h>

SemaphoreHandle_t displayMutex = nullptr;
static auto TAG = "DISPLAY";

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
}

#define ICON_TILES 4

void clearIcons() {
    // Use a more conservative approach similar to stable version
    display.clearBuffer();
    display.sendBuffer();
}

void clearHeader() {
    // Use a more conservative approach similar to stable version
    display.clearBuffer();
    display.sendBuffer();
}

void clearFooter() {
    // Use a more conservative approach similar to stable version
    display.clearBuffer();
    display.sendBuffer();
}

auto clearPage(const bool includeFooter, const bool includeHeader) -> void {
    // Always use full buffer clear for maximum compatibility
    display.clearBuffer();
}

void refreshIcons() {
    // Use partial update for just the icon region if possible
    // For now, send the full buffer but this could be optimized further
    display.sendBuffer();
}

void refreshHeader() {
    // Use partial update for just the header region if possible  
    // For now, send the full buffer but this could be optimized further
    display.sendBuffer();
}

void refreshPage(bool includeFooter, bool includeHeader) {
    // Full page refresh - send the complete buffer
    display.sendBuffer();
}

void refreshFooter() {
    // Use partial update for just the footer region if possible
    // For now, send the full buffer but this could be optimized further  
    display.sendBuffer();
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
