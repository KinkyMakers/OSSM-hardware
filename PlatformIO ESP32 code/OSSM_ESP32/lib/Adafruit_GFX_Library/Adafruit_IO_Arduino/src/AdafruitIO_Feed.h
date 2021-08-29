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
#ifndef ADAFRUITIO_FEED_H
#define ADAFRUITIO_FEED_H

#include "Arduino.h"
#include "Adafruit_MQTT.h"
#include "AdafruitIO_Definitions.h"
#include "AdafruitIO_Data.h"
#include "AdafruitIO_MQTT.h"

// forward declaration
class AdafruitIO;

class AdafruitIO_Feed : public AdafruitIO_MQTT {

  public:
    AdafruitIO_Feed(AdafruitIO *io, const char *name);
    AdafruitIO_Feed(AdafruitIO *io, const char *name, const char *owner);

    ~AdafruitIO_Feed();

    bool save(char *value, double lat=0, double lon=0, double ele=0);
    bool save(bool value, double lat=0, double lon=0, double ele=0);
    bool save(String value, double lat=0, double lon=0, double ele=0);
    bool save(int value, double lat=0, double lon=0, double ele=0);
    bool save(unsigned int value, double lat=0, double lon=0, double ele=0);
    bool save(long value, double lat=0, double lon=0, double ele=0);
    bool save(unsigned long value, double lat=0, double lon=0, double ele=0);
    bool save(float value, double lat=0, double lon=0, double ele=0, int precision=6);
    bool save(double value, double lat=0, double lon=0, double ele=0, int precision=6);

    bool get();

    bool exists();
    bool create();

    void setLocation(double lat, double lon, double ele=0);

    void onMessage(AdafruitIODataCallbackType cb);
    void subCallback(char *val, uint16_t len);

    const char *name;
    const char *owner;

    AdafruitIO_Data *lastValue();
    AdafruitIO_Data *data;

  private:
    AdafruitIODataCallbackType _dataCallback;

    void _init();

    char *_topic;
    char *_get_topic;
    char *_create_url;
    char *_feed_url;

    Adafruit_MQTT_Subscribe *_sub;
    Adafruit_MQTT_Publish *_pub;
    Adafruit_MQTT_Publish *_get_pub;

    AdafruitIO *_io;
    AdafruitIO_Data *_data;

};

#endif // ADAFRUITIO_FEED_H
