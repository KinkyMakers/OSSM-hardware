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
#ifdef ESP8266

#include "AdafruitIO_ESP8266.h"

AdafruitIO_ESP8266::AdafruitIO_ESP8266(const char *user, const char *key, const char *ssid, const char *pass):AdafruitIO(user, key)
{
  _ssid = ssid;
  _pass = pass;
  _client = new WiFiClientSecure;
  _mqtt = new Adafruit_MQTT_Client(_client, _host, _mqtt_port);
  _http = new HttpClient(*_client, _host, _http_port);
}

AdafruitIO_ESP8266::~AdafruitIO_ESP8266()
{
  if(_client)
    delete _client;
  if(_mqtt)
    delete _mqtt;
}

void AdafruitIO_ESP8266::_connect()
{

  delay(100);
  WiFi.begin(_ssid, _pass);
  delay(100);
  _status = AIO_NET_DISCONNECTED;

}

aio_status_t AdafruitIO_ESP8266::networkStatus()
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

const char* AdafruitIO_ESP8266::connectionType()
{
  return "wifi";
}

#endif // ESP8266
