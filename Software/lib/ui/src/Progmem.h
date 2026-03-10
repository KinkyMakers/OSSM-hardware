#ifndef UI_PROGMEM_H
#define UI_PROGMEM_H

#if defined(ARDUINO)
#include <pgmspace.h>
#else
#ifndef PROGMEM
#define PROGMEM
#endif
#endif

#endif  // UI_PROGMEM_H
