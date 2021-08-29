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
#ifndef ADAFRUITIO_FONA_H
#define ADAFRUITIO_FONA_H

#include "Arduino.h"
#include <SoftwareSerial.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_FONA.h"
#include "AdafruitIO.h"
#include "Adafruit_MQTT_FONA.h"

#define FONA_RX  9
#define FONA_TX  8
#define FONA_RST 4
#define FONA_RI  7
#define FONA_BAUD 4800

// all logic in .h to avoid auto compile
class AdafruitIO_FONA : public AdafruitIO {

  public:
    AdafruitIO_FONA(const char *user, const char *key):AdafruitIO(user, key)
    {
      _serial = new SoftwareSerial(FONA_TX, FONA_RX);
      _fona = new Adafruit_FONA(FONA_RST);
      _mqtt = new Adafruit_MQTT_FONA(_fona, _host, _mqtt_port);
      _packetread_timeout = 500;
    }

    void setAPN(FONAFlashStringPtr apn, FONAFlashStringPtr username=0, FONAFlashStringPtr password=0)
    {
      _fona->setGPRSNetworkSettings(apn, username, password);
    }

    aio_status_t AdafruitIO_FONA::networkStatus()
    {
      // return if in a failed state
      if(_status == AIO_NET_CONNECT_FAILED)
        return _status;

      // if we are connected, return
      if(_fona->GPRSstate())
       return AIO_NET_CONNECTED;

      // wait for connection to network
      if(_fona->getNetworkStatus() != 1)
        return AIO_NET_DISCONNECTED;

      _fona->enableGPRS(true);
      return AIO_NET_CONNECTED;
    }

    const char* connectionType()
    {
      return "fona";
    }

  protected:
    uint16_t _mqtt_port = 1883;

    SoftwareSerial *_serial;
    Adafruit_FONA *_fona;

    void _connect()
    {
      // set software serial baud rate
      _serial->begin(FONA_BAUD);

      // if fona can't be found, bail
      if(! _fona->begin(*_serial)) {
        _status = AIO_NET_CONNECT_FAILED;
        return;
      }

      // disable cme error reporting
      _serial->println("AT+CMEE=2");

      _status = AIO_NET_DISCONNECTED;
    }

};

#endif // ADAFRUITIO_FONA_H
