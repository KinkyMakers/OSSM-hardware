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
#if !defined(ARDUINO_SAMD_MKR1000) && defined(ARDUINO_ARCH_SAMD)

#include "AdafruitIO_WINC1500.h"


AdafruitIO_WINC1500::AdafruitIO_WINC1500(const char *user, const char *key, const char *ssid, const char *pass):AdafruitIO(user, key)
{
  _ssid = ssid;
  _pass = pass;
  _mqtt_client = new WiFiSSLClient;
  _mqtt = new Adafruit_MQTT_Client(_mqtt_client, _host, _mqtt_port);
  _http_client = new WiFiSSLClient;
  _http = new HttpClient(*_http_client, _host, _http_port);
}

AdafruitIO_WINC1500::~AdafruitIO_WINC1500()
{
  if(_mqtt_client)
    delete _http_client;
  if(_http_client)
    delete _mqtt_client;
  if(_mqtt)
    delete _mqtt;
  if(_http)
    delete _http;
}

void AdafruitIO_WINC1500::_connect()
{

  WiFi.setPins(WINC_CS, WINC_IRQ, WINC_RST, WINC_EN);

  // no shield? bail
  if(WiFi.status() == WL_NO_SHIELD)
    return;

  WiFi.begin(_ssid, _pass);
  _status = AIO_NET_DISCONNECTED;

}

aio_status_t AdafruitIO_WINC1500::networkStatus()
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

const char* AdafruitIO_WINC1500::connectionType()
{
  return "winc1500";
}

#endif // ARDUINO_ARCH_SAMD
