#ifndef OSSM_SOFTWARE_DISPLAY_H
#define OSSM_SOFTWARE_DISPLAY_H
#include <U8g2lib.h>

#include "constants/Pins.h"

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
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0,
                                                   Pins::Display::oledReset,
                                                   Pins::Remote::displayClock,
                                                   Pins::Remote::displayData);

#endif  // OSSM_SOFTWARE_DISPLAY_H
