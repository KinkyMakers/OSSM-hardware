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
#ifndef ADAFRUITIO_MQTT_H
#define ADAFRUITIO_MQTT_H

#include "Arduino.h"

class AdafruitIO_MQTT {

  public:
    AdafruitIO_MQTT() {}
    virtual void subCallback(char *val, uint16_t len) = 0;

};

#endif // ADAFRUITIO_MQTT_H
