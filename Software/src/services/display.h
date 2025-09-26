#ifndef OSSM_SOFTWARE_DISPLAY_H
#define OSSM_SOFTWARE_DISPLAY_H
#include <U8g2lib.h>

#include "constants/Pins.h"
#include "freertos/semphr.h"
#include "utils/RecursiveMutex.h"

#pragma once
/**
 * Welcome to our display service.
 *
 * Here are some documents that might help you to start drawing with this
 * library:
 *
 * https://github.com/olikraus/u8g2/wiki/u8g2reference
 *
 * And here's a list of all U8G2 fonts:
 *
 * https://github.com/olikraus/u8g2/wiki/fntlistallplain
 */

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
#define OLED_RESET U8X8_PIN_NONE

extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C display;
extern SemaphoreHandle_t displayMutex;

void initDisplay();

void clearIcons();
void refreshIcons();

void clearHeader();
void clearFooter();
void clearPage(bool includeFooter = false, bool includeHeader = false);

void refreshHeader();

void refreshFooter();

void setHeader(String &text);
void setFooter(String &left, String &right);

void refreshPage(bool includeFooter = false, bool includeHeader = false);

int drawWrappedText(int x, int y, const String &text, bool center = false);

#endif  // OSSM_SOFTWARE_DISPLAY_H
