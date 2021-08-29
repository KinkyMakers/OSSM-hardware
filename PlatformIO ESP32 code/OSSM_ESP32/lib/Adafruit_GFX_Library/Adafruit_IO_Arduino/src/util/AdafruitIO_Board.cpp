//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2016 Adafruit Industries
// Authors: Todd Treece
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#include "AdafruitIO_Board.h"

char AdafruitIO_Board::_id[64] = "";

#if defined(ARDUINO_SAMD_MKR1000)
    const char AdafruitIO_Board::_type[] = "mkr1000";
#elif defined(ARDUINO_SAMD_FEATHER_M0)
    const char AdafruitIO_Board::_type[] = "feather_m0";
#elif defined(ARDUINO_AVR_FEATHER32U4)
    const char AdafruitIO_Board::_type[] = "feather_32u4";
#elif defined(ARDUINO_STM32_FEATHER)
    const char AdafruitIO_Board::_type[] = "feather_wiced";
#elif defined(ARDUINO_ARCH_ESP32)
    #include <WiFi.h>
    const char AdafruitIO_Board::_type[] = "esp32";
#elif defined(ESP8266)
    const char AdafruitIO_Board::_type[] = "esp8266";
#else
    const char AdafruitIO_Board::_type[] = "unknown";
#endif

const char* AdafruitIO_Board::type()
{
  return AdafruitIO_Board::_type;
}

#if defined(ARDUINO_ARCH_SAMD)

  char* AdafruitIO_Board::id()
  {
    volatile uint32_t val1, val2, val3, val4;
    volatile uint32_t *ptr1 = (volatile uint32_t *)0x0080A00C;
    val1 = *ptr1;
    volatile uint32_t *ptr = (volatile uint32_t *)0x0080A040;
    val2 = *ptr;
    ptr++;
    val3 = *ptr;
    ptr++;
    val4 = *ptr;

    sprintf(AdafruitIO_Board::_id, "%8x%8x%8x%8x", val1, val2, val3, val4);

    return AdafruitIO_Board::_id;
  }

#elif defined(ARDUINO_ARCH_AVR)

  char* AdafruitIO_Board::id()
  {
    for(int i=0; i < 32; i++) {
      sprintf(&AdafruitIO_Board::_id[i*2],"%02x", boot_signature_byte_get(i));
    }
    return AdafruitIO_Board::_id;
  }

#elif defined(ARDUINO_ARCH_ESP32)

  char* AdafruitIO_Board::id()
  {
    byte mac[6];
    WiFi.macAddress(mac);
    for(int i=0; i < 6; i++) {
      sprintf(&AdafruitIO_Board::_id[i*2],"%02x", mac[i]);
    }
    return AdafruitIO_Board::_id;
  }

#elif defined(ESP8266)

  char* AdafruitIO_Board::id()
  {
    sprintf(AdafruitIO_Board::_id, "%06x", ESP.getChipId());
    return AdafruitIO_Board::_id;
  }

#elif defined(ARDUINO_STM32_FEATHER)

  char* AdafruitIO_Board::id()
  {
    uint32_t* p_unique_id = (uint32_t*) (0x1FFF7A10);
    sprintf(AdafruitIO_Board::_id, "%08lX%08lX%08lX", p_unique_id[2], p_unique_id[1], p_unique_id[0]);
    return AdafruitIO_Board::_id;
  }

#else

  char* AdafruitIO_Board::id()
  {
    strcpy(AdafruitIO_Board::_id, "unknown");
    return AdafruitIO_Board::_id;
  }

#endif
