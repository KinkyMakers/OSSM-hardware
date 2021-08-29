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
#ifndef ADAFRUITIO_ESP32_H
#define ADAFRUITIO_ESP32_H

#ifdef ARDUINO_ARCH_ESP32

#include "Arduino.h"
#include "AdafruitIO.h"
#include <WiFi.h>
#include "WiFiClientSecure.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

class AdafruitIO_ESP32 : public AdafruitIO {

  public:
    AdafruitIO_ESP32(const char *user, const char *key, const char *ssid, const char *pass);
    ~AdafruitIO_ESP32();

    aio_status_t networkStatus();
    const char* connectionType();

  protected:
    void _connect();

    const char *_ssid;
    const char *_pass;

    WiFiClientSecure *_client;

};

#endif //ESP32
#endif // ADAFRUITIO_ESP32_H
