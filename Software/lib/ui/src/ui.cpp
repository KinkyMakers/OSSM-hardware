#include "ui.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

extern "C" {
#include "qrcode.h"
}

namespace ui {

    void clearPage(u8g2_t* u8g2, bool includeFooter, bool includeHeader) {
        int y1 = includeHeader ? 0 : HEADER_HEIGHT;
        int y2 = includeFooter ? SCREEN_HEIGHT : PAGE_BOTTOM;
        u8g2_SetClipWindow(u8g2, 0, y1, SCREEN_WIDTH, y2);
        u8g2_ClearBuffer(u8g2);
    }

    void clearHeader(u8g2_t* u8g2) {
        u8g2_SetClipWindow(u8g2, 0, 0, ICON_AREA_X, HEADER_HEIGHT);
        u8g2_ClearBuffer(u8g2);
    }

    void clearFooter(u8g2_t* u8g2) {
        u8g2_SetClipWindow(u8g2, 0, SCREEN_HEIGHT, SCREEN_WIDTH, SCREEN_HEIGHT);
        u8g2_ClearBuffer(u8g2);
    }

    void clearIcons(u8g2_t* u8g2) {
        u8g2_SetClipWindow(u8g2, ICON_AREA_X, 0, SCREEN_WIDTH, HEADER_HEIGHT);
        u8g2_ClearBuffer(u8g2);
    }

    void setHeader(u8g2_t* u8g2, const char* text) {
        std::string upper(text);
        for (auto& c : upper) c = toupper(c);
        clearHeader(u8g2);
        u8g2_SetFont(u8g2, Font::header);
        u8g2_DrawStr(u8g2, 0, HEADER_HEIGHT, upper.c_str());
        u8g2_SetMaxClipWindow(u8g2);
    }

    void setFooter(u8g2_t* u8g2, const char* left, const char* right) {
        std::string leftUpper(left);
        std::string rightUpper(right);
        for (auto& c : leftUpper) c = toupper(c);
        for (auto& c : rightUpper) c = toupper(c);
        clearFooter(u8g2);
        u8g2_SetFont(u8g2, Font::header);
        u8g2_DrawStr(u8g2, 0, SCREEN_HEIGHT, leftUpper.c_str());
        u8g2_DrawStr(
            u8g2, SCREEN_WIDTH - u8g2_GetStrWidth(u8g2, rightUpper.c_str()) - 8,
            SCREEN_HEIGHT, rightUpper.c_str());
        u8g2_SetMaxClipWindow(u8g2);
    }

    int drawWrappedText(u8g2_t* u8g2, int x, int y, const char* text,
                        bool center, int maxWidth) {
        if (maxWidth <= 0) maxWidth = u8g2_GetDisplayWidth(u8g2) - x;
        int lineHeight = u8g2_GetAscent(u8g2) + 2;
        int fontHeight = u8g2_GetAscent(u8g2);
        int cursorY = y + fontHeight;
        int lineCount = 0;

        std::string word;
        std::string line;
        size_t textLen = strlen(text);

        auto drawLine = [&](const std::string& l) {
            int drawX = x;
            if (center) {
                int lineWidth = u8g2_GetStrWidth(u8g2, l.c_str());
                drawX = (u8g2_GetDisplayWidth(u8g2) - x - lineWidth) / 2 + x;
            }
            u8g2_DrawStr(u8g2, drawX, cursorY, l.c_str());
            cursorY += lineHeight;
            lineCount++;
        };

        for (size_t i = 0; i < textLen; ++i) {
            if (text[i] == '\\' && i + 1 < textLen && text[i + 1] == 'n') {
                if (!word.empty()) {
                    if (!line.empty()) line += ' ';
                    line += word;
                }
                drawLine(line);
                line.clear();
                word.clear();
                i++;
                continue;
            }
            char c = text[i];
            if (c == ' ' || c == '\n' || i == textLen - 1) {
                if (i == textLen - 1 && c != ' ' && c != '\n') {
                    word += c;
                }
                std::string testLine = line;
                if (!testLine.empty()) testLine += ' ';
                testLine += word;
                int testWidth = u8g2_GetStrWidth(u8g2, testLine.c_str());
                if (testWidth > maxWidth && !line.empty()) {
                    drawLine(line);
                    line = word;
                } else {
                    if (!line.empty()) line += ' ';
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
        if (!line.empty()) {
            drawLine(line);
        }
        return lineCount;
    }

    // --- Page Render Functions ---

    void drawTextPage(u8g2_t* u8g2, const TextPage& page) {
        clearPage(u8g2, true, true);
        u8g2_SetMaxClipWindow(u8g2);

        int qrPixelWidth = 0;
        if (page.qrUrl) {
            QRCode qrcode;
            uint8_t qrcodeData[qrcode_getBufferSize(page.qrVersion)];
            qrcode_initText(&qrcode, qrcodeData, page.qrVersion, 0, page.qrUrl);
            int qrSize = qrcode.size * page.qrScale;
            int xOffset = SCREEN_WIDTH - qrSize;
            int yOffset = SCREEN_HEIGHT - qrSize;
            if (xOffset < 0) xOffset = 0;
            if (yOffset < 0) yOffset = 0;
            drawQR(u8g2, page.qrUrl, xOffset, yOffset, page.qrVersion,
                   page.qrScale);
            qrPixelWidth = qrSize;
        }

        int textW =
            qrPixelWidth > 0 ? SCREEN_WIDTH - qrPixelWidth - 4 : SCREEN_WIDTH;
        int y = 0;

        if (page.title) {
            u8g2_SetFont(u8g2, Font::bold);
            int ascent = u8g2_GetAscent(u8g2);
            u8g2_DrawUTF8(u8g2, 0, y + ascent, page.title);
            y += ascent + 2;
            int separatorW = qrPixelWidth > 0 ? textW : 80;
            u8g2_DrawHLine(u8g2, 0, y, separatorW);
            y += 2;
        }

        if (page.subtitle) {
            u8g2_SetFont(u8g2, Font::medium);
            int subW = u8g2_GetUTF8Width(u8g2, page.subtitle);
            if (subW <= textW) {
                int ascent = u8g2_GetAscent(u8g2);
                u8g2_DrawUTF8(u8g2, 0, y + ascent, page.subtitle);
                y += ascent + 2;
            } else {
                u8g2_SetFont(u8g2, Font::bold);
                subW = u8g2_GetUTF8Width(u8g2, page.subtitle);
                int ascent = u8g2_GetAscent(u8g2);
                if (subW <= textW) {
                    u8g2_DrawUTF8(u8g2, 0, y + ascent, page.subtitle);
                    y += ascent + 2;
                } else {
                    int len = strlen(page.subtitle);
                    int half = len / 2;
                    char firstHalf[32] = {0};
                    strncpy(firstHalf, page.subtitle, half);
                    u8g2_DrawUTF8(u8g2, 0, y + ascent, firstHalf);
                    y += ascent + 2;
                    u8g2_DrawUTF8(u8g2, 0, y + ascent, page.subtitle + half);
                    y += ascent + 2;
                }
            }
        }

        if (page.body) {
            if (y > 0) {
                u8g2_SetFont(u8g2, Font::base);
                drawWrappedText(u8g2, 0, y, page.body, false, textW);
            } else {
                drawStr::title(u8g2, page.body);
            }
        }

        if (page.bottomText) {
            u8g2_SetFont(u8g2, Font::base);
            u8g2_DrawUTF8(u8g2, 0, 62, page.bottomText);
        }

        if (page.scrollPercent >= 0) {
            drawShape::scroll(u8g2, page.scrollPercent);
        }
    }

    void drawHelloFrame(u8g2_t* u8g2, const HelloFrame& frame) {
        clearPage(u8g2, true, true);
        u8g2_SetMaxClipWindow(u8g2);
        u8g2_SetFont(u8g2, Font::title);
        int startX = 24;
        int letterSpacing = 20;
        u8g2_DrawUTF8(u8g2, startX, frame.heights[0], "O");
        u8g2_DrawUTF8(u8g2, startX + letterSpacing, frame.heights[1], "S");
        u8g2_DrawUTF8(u8g2, startX + letterSpacing * 2, frame.heights[2], "S");
        u8g2_DrawUTF8(u8g2, startX + letterSpacing * 3, frame.heights[3], "M");
    }

    void drawLogo(u8g2_t* u8g2, const LogoData& data) {
        clearPage(u8g2, true, true);
        u8g2_SetMaxClipWindow(u8g2);
        drawStr::title(u8g2, data.title);
        u8g2_DrawXBMP(u8g2, data.x, data.y, data.w, data.h, data.bitmap);
    }

    void drawPreflight(u8g2_t* u8g2, const PreflightData& data) {
        clearPage(u8g2, true, true);
        u8g2_SetMaxClipWindow(u8g2);
        drawStr::title(u8g2, data.title);

        char speedStr[32];
        snprintf(speedStr, sizeof(speedStr), "%s: %d%%", data.speedLabel,
                 (int)data.speedPercentage);
        drawStr::centered(u8g2, 25, speedStr);
        drawStr::multiLine(u8g2, 0, 40, data.warning);
    }

    void drawPlayControls(u8g2_t* u8g2, const PlayControlsData& data) {
        clearPage(u8g2, true);
        u8g2_SetMaxClipWindow(u8g2);

        if (data.headerText) {
            setHeader(u8g2, data.headerText);
        }

        u8g2_SetFont(u8g2, Font::base);

        if (!data.isStreaming) {
            drawShape::settingBar(
                u8g2, data.speedLabel ? data.speedLabel : strings::speed,
                data.speed);
        } else {
            drawShape::settingBar(u8g2, strings::max, data.speed);
            u8g2_SetFont(u8g2, Font::bold);
            u8g2_DrawUTF8(u8g2, 14, 36,
                          data.speedLabel ? data.speedLabel : strings::speed);
        }

        const char* strokeLabel =
            data.strokeLabel ? data.strokeLabel : strings::stroke;

        if (data.isStrokeEngine) {
            switch (data.activeControl) {
                case PlayControl::STROKE:
                    drawShape::settingBarSmall(u8g2, data.sensation, 125);
                    drawShape::settingBarSmall(u8g2, data.depth, 120);
                    drawShape::settingBar(u8g2, strokeLabel, data.stroke, 118,
                                          0, RIGHT_ALIGNED);
                    break;
                case PlayControl::SENSATION:
                    drawShape::settingBar(u8g2, strings::sensation,
                                          data.sensation, 128, 0, RIGHT_ALIGNED,
                                          10);
                    drawShape::settingBarSmall(u8g2, data.depth, 113);
                    drawShape::settingBarSmall(u8g2, data.stroke, 108);
                    break;
                case PlayControl::DEPTH:
                    drawShape::settingBarSmall(u8g2, data.sensation, 125);
                    drawShape::settingBar(u8g2, strings::depth, data.depth, 123,
                                          0, RIGHT_ALIGNED, 5);
                    drawShape::settingBarSmall(u8g2, data.stroke, 108);
                    break;
                default:
                    break;
            }
        } else if (data.isStreaming) {
            switch (data.activeControl) {
                case PlayControl::BUFFER:
                    drawShape::settingBar(u8g2, strings::buffer, data.buffer,
                                          128, 0, RIGHT_ALIGNED, 15);
                    drawShape::settingBarSmall(u8g2, data.sensation, 113);
                    drawShape::settingBarSmall(u8g2, data.depth, 108);
                    drawShape::settingBarSmall(u8g2, data.stroke, 103);
                    break;
                case PlayControl::STROKE:
                    drawShape::settingBarSmall(u8g2, data.buffer, 125);
                    drawShape::settingBarSmall(u8g2, data.sensation, 120);
                    drawShape::settingBarSmall(u8g2, data.depth, 115);
                    drawShape::settingBar(u8g2, strokeLabel, data.stroke, 113,
                                          0, RIGHT_ALIGNED);
                    break;
                case PlayControl::SENSATION: {
                    drawShape::settingBarSmall(u8g2, data.buffer, 125);
                    drawShape::settingBar(u8g2, strings::max, data.sensation,
                                          123, 0, RIGHT_ALIGNED, 10);
                    u8g2_SetFont(u8g2, Font::bold);
                    const char* accelLabel = strings::accel;
                    int sw = u8g2_GetUTF8Width(u8g2, accelLabel);
                    u8g2_DrawUTF8(u8g2, 99 - sw, 36, accelLabel);
                    drawShape::settingBarSmall(u8g2, data.depth, 108);
                    drawShape::settingBarSmall(u8g2, data.stroke, 103);
                    break;
                }
                case PlayControl::DEPTH:
                    drawShape::settingBarSmall(u8g2, data.buffer, 125);
                    drawShape::settingBarSmall(u8g2, data.sensation, 120);
                    drawShape::settingBar(u8g2, strings::depth, data.depth, 118,
                                          0, RIGHT_ALIGNED, 5);
                    drawShape::settingBarSmall(u8g2, data.stroke, 103);
                    break;
            }
        } else {
            drawShape::settingBar(u8g2, strokeLabel, data.stroke, 118, 0,
                                  RIGHT_ALIGNED);
        }

        short lh3 = 56;
        short lh4 = 64;

        if (!data.isStrokeEngine && !data.isStreaming) {
            u8g2_SetFont(u8g2, Font::small);
            char countStr[16];
            snprintf(countStr, sizeof(countStr), "# %d", data.strokeCount);
            u8g2_DrawUTF8(u8g2, 14, lh4, countStr);
        }

        if (!data.isStrokeEngine && !data.isStreaming && data.distanceStr) {
            int sw = u8g2_GetUTF8Width(u8g2, data.distanceStr);
            u8g2_DrawUTF8(u8g2, 104 - sw, lh3, data.distanceStr);
        }

        if (!data.isStreaming && data.timeStr) {
            int sw = u8g2_GetUTF8Width(u8g2, data.timeStr);
            u8g2_DrawUTF8(u8g2, 104 - sw, lh4, data.timeStr);
        }
    }

    void drawMenu(u8g2_t* u8g2, const MenuData& data) {
        clearPage(u8g2, true, false);
        u8g2_SetMaxClipWindow(u8g2);

        if (data.numItems <= 0 || !data.items) return;

        int leftPadding = 6;
        int fontSize = 8;
        int itemHeight = 20;
        int idx = data.selectedIndex;
        int n = data.numItems;

        drawShape::scroll(u8g2, scrollPercent(idx, n));

        int textClipRight = 120;
        u8g2_SetClipWindow(u8g2, 0, 0, textClipRight, SCREEN_HEIGHT);

        u8g2_SetFont(u8g2, Font::base);
        if (n > 1) {
            int prevIdx = (idx - 1 + n) % n;
            u8g2_DrawUTF8(u8g2, leftPadding, itemHeight * 1,
                          data.items[prevIdx]);

            int nextIdx = (idx + 1) % n;
            u8g2_DrawUTF8(u8g2, leftPadding, itemHeight * 3,
                          data.items[nextIdx]);
        }

        u8g2_SetFont(u8g2, Font::bold);
        u8g2_DrawUTF8(u8g2, leftPadding, itemHeight * 2, data.items[idx]);

        u8g2_SetMaxClipWindow(u8g2);

        int visibleItems = 3;
        u8g2_DrawRFrame(
            u8g2, 0,
            itemHeight * (visibleItems / 2) - (fontSize - itemHeight) / 2, 120,
            itemHeight, 2);

        u8g2_DrawLine(u8g2, 2, 2 + fontSize / 2 + 2 * itemHeight, 119,
                      2 + fontSize / 2 + 2 * itemHeight);
        u8g2_DrawLine(u8g2, 120, 4 + fontSize / 2 + itemHeight, 120,
                      1 + fontSize / 2 + 2 * itemHeight);

        u8g2_SetMaxClipWindow(u8g2);
    }

    void drawQR(u8g2_t* u8g2, const char* text, int originX, int originY,
                uint8_t version, int scale) {
        QRCode qrcode;
        uint8_t qrcodeData[qrcode_getBufferSize(version)];
        qrcode_initText(&qrcode, qrcodeData, version, 0, text);

        for (uint8_t y = 0; y < qrcode.size; y++) {
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    if (scale == 1) {
                        u8g2_DrawPixel(u8g2, originX + x, originY + y);
                    } else {
                        u8g2_DrawBox(u8g2, originX + x * scale,
                                     originY + y * scale, scale, scale);
                    }
                }
            }
        }
    }

    void drawHeaderIcons(u8g2_t* u8g2, const HeaderIconsData& data) {
        clearIcons(u8g2);
        u8g2_SetMaxClipWindow(u8g2);
        u8g2_SetFont(u8g2, Font::icons);

        switch (data.wifi) {
            case WifiStatus::DISCONNECTED:
                u8g2_DrawGlyph(u8g2, WIFI_ICON_X, ICON_Y, IconGlyph::WIFI_OFF);
                break;
            case WifiStatus::CONNECTING:
                u8g2_DrawGlyph(u8g2, WIFI_ICON_X, ICON_Y,
                               IconGlyph::WIFI_CONNECTING);
                break;
            case WifiStatus::CONNECTED:
                u8g2_DrawGlyph(u8g2, WIFI_ICON_X, ICON_Y,
                               IconGlyph::WIFI_CONNECTED);
                break;
            case WifiStatus::ERROR: {
                u8g2_DrawGlyph(u8g2, WIFI_ICON_X, ICON_Y, IconGlyph::WIFI_OFF);
                constexpr int ex = WIFI_ICON_X + 11;
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 5);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 4);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 3);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 1);
                break;
            }
        }

        constexpr int BLE_SUB_X = BLE_ICON_X - 3;
        constexpr int BLE_IND_X = BLE_SUB_X + 7;

        constexpr int BLE_Y = ICON_Y - 1;
        constexpr int BLE_SUB_Y = ICON_Y - 1;

        switch (data.ble) {
            case BleStatus::CONNECTED:
                u8g2_DrawGlyph(u8g2, BLE_ICON_X, BLE_Y,
                               IconGlyph::BLE_SMALL);
                break;
            case BleStatus::DISCONNECTED: {
                constexpr int cx = BLE_ICON_X + 4;
                constexpr int cy = ICON_Y - 5;
                u8g2_DrawCircle(u8g2, cx, cy, 3, U8G2_DRAW_ALL);
                break;
            }
            case BleStatus::CONNECTING:
            case BleStatus::ADVERTISING:
                u8g2_DrawGlyph(u8g2, BLE_SUB_X, BLE_Y, IconGlyph::BLE_SMALL);
                u8g2_DrawPixel(u8g2, BLE_IND_X, BLE_SUB_Y);
                u8g2_DrawPixel(u8g2, BLE_IND_X + 2, BLE_SUB_Y);
                u8g2_DrawPixel(u8g2, BLE_IND_X + 4, BLE_SUB_Y);
                break;
            case BleStatus::ERROR: {
                u8g2_DrawGlyph(u8g2, BLE_SUB_X, BLE_Y, IconGlyph::BLE_SMALL);
                constexpr int ex = BLE_ICON_X + 7;
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 7);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 6);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 5);
                u8g2_DrawPixel(u8g2, ex, ICON_Y - 3);
                break;
            }
        }
    }

}  // namespace ui
