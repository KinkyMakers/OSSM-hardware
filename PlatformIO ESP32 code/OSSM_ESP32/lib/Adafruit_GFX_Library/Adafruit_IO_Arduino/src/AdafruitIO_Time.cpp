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
#include "AdafruitIO_Time.h"
#include "AdafruitIO.h"

AdafruitIO_Time::AdafruitIO_Time(AdafruitIO *io, aio_time_format_t f):AdafruitIO_MQTT()
{
  _io = io;
  _sub = 0;
  _dataCallback = 0;
  format = f;

  _init();
}

AdafruitIO_Time::~AdafruitIO_Time()
{
  if(_sub)
    delete _sub;

  if(data)
    delete data;

  if(_topic)
    free(_topic);
}

void AdafruitIO_Time::onMessage(AdafruitIOTimeCallbackType cb)
{
  _dataCallback = cb;
}

void AdafruitIO_Time::subCallback(char *val, uint16_t len)
{
  data = val;

  // call callback with data
  if(_dataCallback)
    _dataCallback(data, len);
}

void AdafruitIO_Time::_init()
{

  // dynamically allocate memory for mqtt topic and REST URLs
  const char *formatString;

  switch (format) {
    case AIO_TIME_SECONDS:
      formatString = "seconds";
      break;
    case AIO_TIME_MILLIS:
      formatString = "millis";
      break;
    case AIO_TIME_ISO:
      formatString = "ISO-8601";
      break;
    default:
      formatString = "seconds";
      break;
  }

  _topic = (char *) malloc(sizeof(char) * (strlen(formatString) + 6)); // 6 extra chars for "time/" and null termination

  if(_topic) {

    // build topic string
    strcpy(_topic, "time/");
    strcat(_topic, formatString);

    // setup subscription
    _sub = new Adafruit_MQTT_Subscribe(_io->_mqtt, _topic);
    _io->_mqtt->subscribe(_sub);
    _sub->setCallback(this, &AdafruitIO_MQTT::subCallback);

  } else {

    // malloc failed
    _topic = 0;
    _sub = 0;
    data = 0;

  }

}
