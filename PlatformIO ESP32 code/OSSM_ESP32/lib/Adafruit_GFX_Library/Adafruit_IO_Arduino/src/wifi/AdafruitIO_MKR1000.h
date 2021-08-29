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
#ifndef ADAFRUITIO_MKR1000_H
#define ADAFRUITIO_MKR1000_H

#if defined(ARDUINO_SAMD_MKR1000)

#include "Arduino.h"
#include "AdafruitIO.h"
#include "SPI.h"
#include "WiFi101.h"
#include "WiFiSSLClient.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

class AdafruitIO_MKR1000 : public AdafruitIO {

  public:
    AdafruitIO_MKR1000(const char *user, const char *key, const char *ssid, const char *pass);
    ~AdafruitIO_MKR1000();

    aio_status_t networkStatus();
    const char* connectionType();

  protected:
    void _connect();

    const char *_ssid;
    const char *_pass;

    WiFiSSLClient *_client;

};

#endif // ARDUINO_ARCH_SAMD

#endif // ADAFRUITIO_MKR1000_H
