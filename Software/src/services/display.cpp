#include "display.h"

ESP32RecursiveMutex displayMutex;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0,
                                           Pins::Display::oledReset,
                                           Pins::Remote::displayClock,
                                           Pins::Remote::displayData); 