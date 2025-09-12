#include "display.h"

ESP32RecursiveMutex displayMutex;

#ifdef AJ_DEVELOPMENT_HARDWARE
static auto ROTATION = U8G2_R2;
#else
static auto ROTATION = U8G2_R0;
#endif

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(ROTATION, Pins::Display::oledReset,
                                            Pins::Remote::displayClock,
                                            Pins::Remote::displayData);
