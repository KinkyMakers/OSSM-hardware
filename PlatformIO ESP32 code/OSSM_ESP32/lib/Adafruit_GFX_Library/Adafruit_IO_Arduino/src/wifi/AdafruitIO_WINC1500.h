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
#ifndef ADAFRUITIO_WINC1500_H
#define ADAFRUITIO_WINC1500_H

#if !defined(ARDUINO_SAMD_MKR1000) && defined(ARDUINO_ARCH_SAMD)

#include "Arduino.h"
#include "AdafruitIO.h"
#include "SPI.h"
#include "WiFi101.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

// feather wifi m0
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2

class AdafruitIO_WINC1500 : public AdafruitIO {

  public:
    AdafruitIO_WINC1500(const char *user, const char *key, const char *ssid, const char *pass);
    ~AdafruitIO_WINC1500();

    aio_status_t networkStatus();
    const char* connectionType();

  protected:
    void _connect();
    const char *_ssid;
    const char *_pass;

    WiFiSSLClient *_http_client;
    WiFiSSLClient *_mqtt_client;

};

#endif // ARDUINO_ARCH_SAMD

#endif // ADAFRUITIO_WINC1500_H
