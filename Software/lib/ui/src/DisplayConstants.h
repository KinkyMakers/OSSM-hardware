#ifndef UI_DISPLAY_CONSTANTS_H
#define UI_DISPLAY_CONSTANTS_H

extern "C" {
#include "clib/u8g2.h"
}

namespace ui {

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;

constexpr int HEADER_HEIGHT = 8;
constexpr int FOOTER_HEIGHT = 8;
constexpr int PAGE_TOP = HEADER_HEIGHT;
constexpr int PAGE_BOTTOM = SCREEN_HEIGHT - FOOTER_HEIGHT;

constexpr int ICON_TILES = 4;
constexpr int ICON_AREA_X = SCREEN_WIDTH - 8 * ICON_TILES;

namespace Font {
static const uint8_t* bold = u8g2_font_helvB08_tf;
static const uint8_t* base = u8g2_font_helvR08_tf;
static const uint8_t* small = u8g2_font_6x10_tf;
static const uint8_t* header = u8g2_font_spleen5x8_mu;
static const uint8_t* title = u8g2_font_maniac_tf;
static const uint8_t* medium = u8g2_font_helvB10_tf;
static const uint8_t* pairingCode = u8g2_font_helvB14_tf;
static const uint8_t* icons = u8g2_font_siji_t_6x10;
}  // namespace Font

namespace IconGlyph {
constexpr uint16_t WIFI_OFF = 0xe218;
constexpr uint16_t WIFI_CONNECTING = 0xe219;
constexpr uint16_t WIFI_CONNECTED = 0xe21a;
constexpr uint16_t WIFI_ERROR = 0xe21b;
constexpr uint16_t BLE_CONNECTED = 0xe00b;
constexpr uint16_t BLE_SMALL = 0xe0b0;
constexpr uint16_t EXCLAMATION = 0xe0b3;
}  // namespace IconGlyph

constexpr int WIFI_ICON_X = 104;
constexpr int BLE_ICON_X = 118;
constexpr int ICON_Y = 8;

}  // namespace ui

#endif  // UI_DISPLAY_CONSTANTS_H
