//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2016 Adafruit Industries
// Authors: Tony DiCola, Todd Treece
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#ifndef ADAFRUITIO_WICED_H
#define ADAFRUITIO_WICED_H

#ifdef ARDUINO_STM32_FEATHER

#include "Arduino.h"
#include "AdafruitIO.h"
#include <adafruit_feather.h>
#include "AdafruitIO_WICED_SSL.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

class AdafruitIO_WICED : public AdafruitIO {

  public:
    AdafruitIO_WICED(const char *user, const char *key, const char *ssid, const char *pass);
    ~AdafruitIO_WICED();

    aio_status_t networkStatus();
    const char* connectionType();

  protected:
    void _connect();
    const char *_ssid;
    const char *_pass;
    AdafruitIO_WICED_SSL *_client;

};

#endif // ARDUINO_STM32_FEATHER

#endif // ADAFRUITIO_WICED_H
