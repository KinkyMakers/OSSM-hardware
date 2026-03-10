#ifndef UI_DRAW_EXTENSIONS_H
#define UI_DRAW_EXTENSIONS_H

#include <cmath>
#include <cstring>
#include <initializer_list>

#include "DisplayConstants.h"

namespace ui {

struct Point {
    u8g2_uint_t x;
    u8g2_uint_t y;
};

enum Alignment { LEFT_ALIGNED, RIGHT_ALIGNED };

namespace drawStr {

static void centered(u8g2_t* u8g2, int y, const char* str) {
    const char* end = str + strlen(str);
    while (end > str && *(end - 1) == ' ') --end;
    size_t trimmedLen = end - str;

    char trimmed[128];
    if (trimmedLen < sizeof(trimmed)) {
        memcpy(trimmed, str, trimmedLen);
        trimmed[trimmedLen] = '\0';
    } else {
        strncpy(trimmed, str, sizeof(trimmed) - 1);
        trimmed[sizeof(trimmed) - 1] = '\0';
    }

    int x = (u8g2_GetDisplayWidth(u8g2) - u8g2_GetUTF8Width(u8g2, trimmed)) / 2;
    u8g2_DrawUTF8(u8g2, x, y, trimmed);
}

static void multiLine(u8g2_t* u8g2, int x, int y, const char* string,
                       int lineHeight = 12) {
    u8g2_SetFont(u8g2, Font::base);
    int displayWidth = u8g2_GetDisplayWidth(u8g2);
    int currentLineWidth = 0;
    char glyph[5] = {0};
    const char* currentChar = string;
    const char* lastSpace = nullptr;
    const char* str = string;

    // NOLINTBEGIN(hicpp-signed-bitwise)
    auto charLen = [](unsigned char c) -> int {
        if ((c & 0x80) == 0) return 1;
        if ((c & 0xE0) == 0xC0) return 2;
        if ((c & 0xF0) == 0xE0) return 3;
        if ((c & 0xF8) == 0xF0) return 4;
        return 1;
    };
    // NOLINTEND(hicpp-signed-bitwise)

    while (*currentChar) {
        while (x == 0 && (*str == ' ' || *str == '\n')) {
            if (currentChar == str++) ++currentChar;
        }
        int cl = charLen(*currentChar);
        strncpy(glyph, currentChar, cl);
        glyph[cl] = 0;
        currentChar += cl;
        currentLineWidth += u8g2_GetStrWidth(u8g2, glyph);
        if (*glyph == ' ')
            lastSpace = currentChar - cl;
        else
            ++currentLineWidth;
        if (*glyph == '\n' || x + currentLineWidth > displayWidth) {
            int startingPosition = x;
            while (str < (lastSpace ? lastSpace : currentChar)) {
                cl = charLen(*str);
                strncpy(glyph, str, cl);
                glyph[cl] = 0;
                x += u8g2_DrawUTF8(u8g2, x, y, glyph);
                str += cl;
            }
            currentLineWidth -= x - startingPosition;
            y += lineHeight;
            x = 0;
            lastSpace = nullptr;
        }
    }
    while (*str) {
        int cl = charLen(*str);
        strncpy(glyph, str, cl);
        glyph[cl] = 0;
        x += u8g2_DrawUTF8(u8g2, x, y, glyph);
        str += cl;
    }
}

static void title(u8g2_t* u8g2, const char* str) {
    u8g2_SetFont(u8g2, Font::bold);
    u8g2_DrawUTF8(u8g2, 0, 8, str);
}

}  // namespace drawStr

namespace drawShape {

static void scroll(u8g2_t* u8g2, long position) {
    if (position < 0) position = 0;
    if (position > 100) position = 100;
    int topMargin = 10;
    int scrollbarHeight = 64 - topMargin;
    int scrollbarWidth = 3;
    int scrollbarX = 125;
    int scrollbarY = (64 - scrollbarHeight + topMargin) / 2;
    for (int i = 0; i < scrollbarHeight; i += 4) {
        u8g2_DrawHLine(u8g2, scrollbarX + 1, scrollbarY + i, 1);
    }
    int rectY =
        scrollbarY + (scrollbarHeight - scrollbarWidth) * position / 100;
    u8g2_DrawBox(u8g2, scrollbarX, rectY, scrollbarWidth, scrollbarWidth);
}

static void settingBar(u8g2_t* u8g2, const char* name, float value, int x = 0,
                        int y = 0, Alignment alignment = LEFT_ALIGNED,
                        int textPadding = 0, float minValue = 0,
                        float maxValue = 100) {
    int w = 10;
    int h = 50;
    int padding = 4;
    int lh1 = 10;

    auto clamp = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    float scaledValue = clamp(value, minValue, maxValue) / maxValue * 100;
    int boxHeight = (int)ceil(h * scaledValue / 100);
    int barStartX = (alignment == LEFT_ALIGNED) ? x : x - w;
    int textStartX =
        (alignment == LEFT_ALIGNED)
            ? x + w + padding + textPadding
            : x - u8g2_GetUTF8Width(u8g2, name) - padding - w - textPadding;
    int barTopY = u8g2_GetDisplayHeight(u8g2) - y - h;
    int barBottomY = u8g2_GetDisplayHeight(u8g2) - y;
    int barFillY = barBottomY - boxHeight;

    u8g2_DrawBox(u8g2, barStartX, barFillY, w, boxHeight);
    u8g2_DrawFrame(u8g2, barStartX, barTopY, w, h);
    u8g2_SetFont(u8g2, Font::bold);
    u8g2_DrawUTF8(u8g2, textStartX, barTopY + lh1, name);

    int firstQuartile = barTopY + h * 3 / 4;
    int half = barTopY + h / 2;
    int thirdQuartile = barTopY + h / 4;

    if (scaledValue >= 75) u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawPixel(u8g2, barStartX + 3, thirdQuartile);
    u8g2_DrawPixel(u8g2, barStartX + 6, thirdQuartile);
    if (scaledValue >= 50) u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawPixel(u8g2, barStartX + 3, half);
    u8g2_DrawPixel(u8g2, barStartX + 6, half);
    if (scaledValue >= 25) u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawPixel(u8g2, barStartX + 3, firstQuartile);
    u8g2_DrawPixel(u8g2, barStartX + 6, firstQuartile);
    u8g2_SetDrawColor(u8g2, 1);
}

static void settingBarSmall(u8g2_t* u8g2, float value, int x = 0, int y = 0,
                             float minValue = 0, float maxValue = 100) {
    int w = 3;
    int mid = (w - 1) / 2;
    int h = 50;

    auto clamp = [](float v, float lo, float hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    float scaledValue = clamp(value, minValue, maxValue) / maxValue * 100;
    int boxHeight = (int)ceil(h * scaledValue / 100);
    int barTopY = u8g2_GetDisplayHeight(u8g2) - y - h;
    int barFillY = u8g2_GetDisplayHeight(u8g2) - y - boxHeight;

    auto clampInt = [](int v, int lo, int hi) {
        return v < lo ? lo : (v > hi ? hi : v);
    };
    int lineH = boxHeight > 0 ? clampInt(h - boxHeight - 2, 0, h) : h;
    u8g2_DrawVLine(u8g2, x + mid, barTopY, lineH);
    u8g2_DrawBox(u8g2, x, barFillY, w, boxHeight);
}

static void lines(u8g2_t* u8g2, std::initializer_list<Point> points) {
    auto it = points.begin();
    Point start = *it;
    for (++it; it != points.end(); ++it) {
        Point end = *it;
        u8g2_DrawLine(u8g2, start.x, start.y, end.x, end.y);
        start = end;
    }
}

}  // namespace drawShape

}  // namespace ui

#endif  // UI_DRAW_EXTENSIONS_H
