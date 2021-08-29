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
#ifdef ARDUINO_STM32_FEATHER

#include "AdafruitIO_WICED.h"

AdafruitIO_WICED::AdafruitIO_WICED(const char *user, const char *key, const char *ssid, const char *pass):AdafruitIO(user, key)
{
  _ssid = ssid;
  _pass = pass;
  _client = new AdafruitIO_WICED_SSL;
  _mqtt = new Adafruit_MQTT_Client(_client, _host, _mqtt_port);
  _http = new HttpClient(*_client, _host, _http_port);
}

AdafruitIO_WICED::~AdafruitIO_WICED()
{
  if(_client)
    delete _client;
  if(_mqtt)
    delete _mqtt;
}

void AdafruitIO_WICED::_connect()
{
  Feather.connect(_ssid, _pass);
  _status = AIO_NET_DISCONNECTED;
}

aio_status_t AdafruitIO_WICED::networkStatus()
{
  if(Feather.connected())
    return AIO_NET_CONNECTED;

  // if granular status is needed, we can
  // check Feather.errno() codes:
  // https://learn.adafruit.com/introducing-the-adafruit-wiced-feather-wifi/constants#err-t
  // for now we will try connecting again and return disconnected status
  Feather.connect(_ssid, _pass);
  return AIO_NET_DISCONNECTED;
}

const char* AdafruitIO_WICED::connectionType()
{
  return "wifi";
}

#endif // ARDUINO_STM32_FEATHER
