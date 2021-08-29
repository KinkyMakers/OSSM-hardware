//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2017 Adafruit Industries
// Authors: Todd Treece
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#ifndef ADAFRUITIO_BOARD_H
#define ADAFRUITIO_BOARD_H

#include "Arduino.h"

#if defined(ARDUINO_ARCH_AVR)
  #include <avr/boot.h>
#endif

class AdafruitIO_Board {

  public:

    static char _id[64];
    static char* id();

    static const char _type[];
    static const char* type();

};

#endif // ADAFRUITIO_BOARD_H
