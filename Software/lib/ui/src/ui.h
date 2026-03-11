#ifndef UI_H
#define UI_H

#include "DisplayConstants.h"
#include "DisplayTypes.h"
#include "DrawExtensions.h"
#include "TextPages.h"

extern "C" {
#include "qrcode.h"
}

namespace ui {

void clearPage(u8g2_t* u8g2, bool includeFooter = false,
               bool includeHeader = false);
void clearHeader(u8g2_t* u8g2);
void clearFooter(u8g2_t* u8g2);
void clearIcons(u8g2_t* u8g2);

void setHeader(u8g2_t* u8g2, const char* text);
void setFooter(u8g2_t* u8g2, const char* left, const char* right);

int drawWrappedText(u8g2_t* u8g2, int x, int y, const char* text,
                    bool center = false, int maxWidth = 0);

void drawHelloFrame(u8g2_t* u8g2, const HelloFrame& frame,
                    const char* version = nullptr);
void drawLogo(u8g2_t* u8g2, const LogoData& data);
void drawPreflight(u8g2_t* u8g2, const PreflightData& data);
void drawPlayControls(u8g2_t* u8g2, const PlayControlsData& data);
void drawMenu(u8g2_t* u8g2, const MenuData& data);
void drawQR(u8g2_t* u8g2, const char* text, int originX, int originY,
            uint8_t version = 3, int scale = 2);
void drawHeaderIcons(u8g2_t* u8g2, const HeaderIconsData& data);
void drawTextPage(u8g2_t* u8g2, const TextPage& page);

}  // namespace ui

#endif  // UI_H
