//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2016 Adafruit Industries
// Author: Todd Treece
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#ifndef ADAFRUITIO_GROUP_H
#define ADAFRUITIO_GROUP_H

#include "Arduino.h"
#include "Adafruit_MQTT.h"
#include "AdafruitIO_Definitions.h"
#include "AdafruitIO_Data.h"
#include "AdafruitIO_MQTT.h"

// forward declaration
class AdafruitIO;

class AdafruitIO_Group : public AdafruitIO_MQTT {

  public:
    AdafruitIO_Group(AdafruitIO *io, const char *name);
    ~AdafruitIO_Group();

    void set(const char *feed, char *value);
    void set(const char *feed, bool value);
    void set(const char *feed, String value);
    void set(const char *feed, int value);
    void set(const char *feed, unsigned int value);
    void set(const char *feed, long value);
    void set(const char *feed, unsigned long value);
    void set(const char *feed, float value);
    void set(const char *feed, double value);

    bool save();
    bool get();

    void setLocation(double lat=0, double lon=0, double ele=0);

    bool exists();
    bool create();

    void onMessage(AdafruitIODataCallbackType cb);
    void onMessage(const char *feed, AdafruitIODataCallbackType cb);

    void subCallback(char *val, uint16_t len);
    void call(AdafruitIO_Data *d);

    const char *name;
    const char *owner;

    AdafruitIO_Data *data;
    AdafruitIO_Data* getFeed(const char *feed);

  private:
    void _init();

    char *_topic;
    char *_get_topic;
    char *_create_url;
    char *_group_url;

    Adafruit_MQTT_Subscribe *_sub;
    Adafruit_MQTT_Publish *_pub;
    Adafruit_MQTT_Publish *_get_pub;

    AdafruitIO *_io;
    AdafruitIOGroupCallback *_groupCallback;

    double _lat,
           _lon,
           _ele;

};

#endif // ADAFRUITIO_GROUP_H
