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
#if defined(ARDUINO_SAMD_MKR1000)

#include "AdafruitIO_MKR1000.h"

AdafruitIO_MKR1000::AdafruitIO_MKR1000(const char *user, const char *key, const char *ssid, const char *pass):AdafruitIO(user, key)
{
  _ssid = ssid;
  _pass = pass;
  _client = new WiFiSSLClient;
  _mqtt = new Adafruit_MQTT_Client(_client, _host, _mqtt_port);
  _http = new HttpClient(*_client, _host, _http_port);
}

AdafruitIO_MKR1000::~AdafruitIO_MKR1000()
{
  if(_client)
    delete _client;
  if(_mqtt)
    delete _mqtt;
}

void AdafruitIO_MKR1000::_connect()
{

  // no shield? bail
  if(WiFi.status() == WL_NO_SHIELD)
    return;

  WiFi.begin(_ssid, _pass);
  _status = AIO_NET_DISCONNECTED;

}

aio_status_t AdafruitIO_MKR1000::networkStatus()
{

  switch(WiFi.status()) {
    case WL_CONNECTED:
      return AIO_NET_CONNECTED;
    case WL_CONNECT_FAILED:
      return AIO_NET_CONNECT_FAILED;
    case WL_IDLE_STATUS:
      return AIO_IDLE;
    default:
      return AIO_NET_DISCONNECTED;
  }

}

const char* AdafruitIO_MKR1000::connectionType()
{
  return "wifi";
}

#endif // ARDUINO_ARCH_SAMD
