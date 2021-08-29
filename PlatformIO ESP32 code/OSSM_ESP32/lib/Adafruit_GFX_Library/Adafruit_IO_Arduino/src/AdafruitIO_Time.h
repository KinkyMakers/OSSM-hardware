//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2016 Adafruit Industries
// Authors: Tony DiCola, Todd Treece, Adam Bachman
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#ifndef ADAFRUITIO_TIME_H
#define ADAFRUITIO_TIME_H

#include "Arduino.h"
#include "Adafruit_MQTT.h"
#include "AdafruitIO_Definitions.h"
#include "AdafruitIO_MQTT.h"

// forward declaration
class AdafruitIO;

typedef void (*AdafruitIOTimeCallbackType)(char *value, uint16_t len);

class AdafruitIO_Time : public AdafruitIO_MQTT {

  public:
    AdafruitIO_Time(AdafruitIO *io, aio_time_format_t f);
    ~AdafruitIO_Time();
    void onMessage(AdafruitIOTimeCallbackType cb);
    void subCallback(char *val, uint16_t len);
    char *data;
    aio_time_format_t format;

  private:
    AdafruitIOTimeCallbackType _dataCallback;
    void _init();
    char *_topic;
    Adafruit_MQTT_Subscribe *_sub;
    AdafruitIO *_io;

};

#endif // ADAFRUITIO_FEED_H
