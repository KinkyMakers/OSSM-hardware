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
#include "AdafruitIO.h"

AdafruitIO::AdafruitIO(const char *user, const char *key)
{
  _mqtt = 0;
  _http = 0;
  _username = user;
  _key = key;
  _err_topic = 0;
  _throttle_topic = 0;
  _err_sub = 0;
  _throttle_sub = 0;
  _packetread_timeout = 100;
  _user_agent = 0;

  _init();
}

void errorCallback(char *err, uint16_t len)
{
  AIO_ERROR_PRINTLN();
  AIO_ERROR_PRINT("ERROR: ");
  AIO_ERROR_PRINTLN(err);
  AIO_ERROR_PRINTLN();
}

void AdafruitIO::connect()
{

  AIO_DEBUG_PRINTLN("AdafruitIO::connect()");

  if(_err_sub) {
    // setup error sub
    _err_sub = new Adafruit_MQTT_Subscribe(_mqtt, _err_topic);
    _mqtt->subscribe(_err_sub);
    _err_sub->setCallback(errorCallback);
  }

  if(_throttle_sub) {
    // setup throttle sub
    _throttle_sub = new Adafruit_MQTT_Subscribe(_mqtt, _throttle_topic);
    _mqtt->subscribe(_throttle_sub);
    _throttle_sub->setCallback(errorCallback);
  }

  _connect();

}

AdafruitIO::~AdafruitIO()
{
  if(_err_topic)
    free(_err_topic);

  if(_throttle_topic)
    free(_throttle_topic);

  if(_err_sub)
    delete _err_sub;

  if(_throttle_sub)
    delete _throttle_sub;
}

AdafruitIO_Feed* AdafruitIO::feed(const char* name)
{
  return new AdafruitIO_Feed(this, name);
}

AdafruitIO_Feed* AdafruitIO::feed(const char* name, const char* owner)
{
  return new AdafruitIO_Feed(this, name, owner);
}

AdafruitIO_Time* AdafruitIO::time(aio_time_format_t format)
{
  return new AdafruitIO_Time(this, format);
}

AdafruitIO_Group* AdafruitIO::group(const char* name)
{
  return new AdafruitIO_Group(this, name);
}

AdafruitIO_Dashboard* AdafruitIO::dashboard(const char* name)
{
  return new AdafruitIO_Dashboard(this, name);
}

void AdafruitIO::_init()
{

  // we have never pinged, so set last ping to now
  _last_ping = millis();

  // dynamically allocate memory for err topic
  _err_topic = (char *)malloc(sizeof(char) * (strlen(_username) + strlen(AIO_ERROR_TOPIC) + 1));

  if(_err_topic) {

    // build error topic
    strcpy(_err_topic, _username);
    strcat(_err_topic, AIO_ERROR_TOPIC);

  } else {

    // malloc failed
    _err_topic = 0;

  }

  // dynamically allocate memory for throttle topic
  _throttle_topic = (char *)malloc(sizeof(char) * (strlen(_username) + strlen(AIO_THROTTLE_TOPIC) + 1));

  if(_throttle_topic) {

    // build throttle topic
    strcpy(_throttle_topic, _username);
    strcat(_throttle_topic, AIO_THROTTLE_TOPIC);

  } else {

    // malloc failed
    _throttle_topic = 0;

  }

}

const __FlashStringHelper* AdafruitIO::statusText()
{
  switch(_status) {

    // CONNECTING
    case AIO_IDLE: return F("Idle. Waiting for connect to be called...");
    case AIO_NET_DISCONNECTED: return F("Network disconnected.");
    case AIO_DISCONNECTED: return F("Disconnected from Adafruit IO.");

    // FAILURE
    case AIO_NET_CONNECT_FAILED: return F("Network connection failed.");
    case AIO_CONNECT_FAILED: return F("Adafruit IO connection failed.");
    case AIO_FINGERPRINT_INVALID: return F("Adafruit IO SSL fingerprint verification failed.");
    case AIO_AUTH_FAILED: return F("Adafruit IO authentication failed.");

    // SUCCESS
    case AIO_NET_CONNECTED: return F("Network connected.");
    case AIO_CONNECTED: return F("Adafruit IO connected.");
    case AIO_CONNECTED_INSECURE: return F("Adafruit IO connected. **THIS CONNECTION IS INSECURE** SSL/TLS not supported for this platform.");
    case AIO_FINGERPRINT_UNSUPPORTED: return F("Adafruit IO connected over SSL/TLS. Fingerprint verification unsupported.");
    case AIO_FINGERPRINT_VALID: return F("Adafruit IO connected over SSL/TLS. Fingerprint valid.");

    default: return F("Unknown status code");

  }
}

void AdafruitIO::run(uint16_t busywait_ms)
{
  // loop until we have a connection
  while(mqttStatus() != AIO_CONNECTED){}

  if(busywait_ms > 0)
    _packetread_timeout = busywait_ms;

  _mqtt->processPackets(_packetread_timeout);

  // ping to keep connection alive if needed
  if(millis() > (_last_ping + AIO_PING_INTERVAL)) {
    _mqtt->ping();
    _last_ping = millis();
  }
}

aio_status_t AdafruitIO::status()
{
  aio_status_t net_status = networkStatus();

  // if we aren't connected, return network status
  if(net_status != AIO_NET_CONNECTED) {
    _status = net_status;
    return _status;
  }

  // check mqtt status and return
  _status = mqttStatus();
  return _status;
}

char* AdafruitIO::boardID()
{
  return AdafruitIO_Board::id();
}

const char* AdafruitIO::boardType()
{
  return AdafruitIO_Board::type();
}

char* AdafruitIO::version()
{
  sprintf(_version, "%d.%d.%d", ADAFRUITIO_VERSION_MAJOR, ADAFRUITIO_VERSION_MINOR, ADAFRUITIO_VERSION_PATCH);
  return _version;
}

char* AdafruitIO::userAgent()
{
  if(!_user_agent) {
    _user_agent = (char *)malloc(sizeof(char) * (strlen(version()) + strlen(boardType()) + strlen(connectionType()) + 24));
    strcpy(_user_agent, "AdafruitIO-Arduino/");
    strcat(_user_agent, version());
    strcat(_user_agent, " (");
    strcat(_user_agent, boardType());
    strcat(_user_agent, "-");
    strcat(_user_agent, connectionType());
    strcat(_user_agent, ")");
  }
  return _user_agent;
}

aio_status_t AdafruitIO::mqttStatus()
{
  // if the connection failed,
  // return so we don't hammer IO
  if(_status == AIO_CONNECT_FAILED)
  {
    AIO_ERROR_PRINT("mqttStatus() failed to connect");
    AIO_ERROR_PRINTLN(_mqtt->connectErrorString(_status));
    return _status;
  }

  if(_mqtt->connected())
    return AIO_CONNECTED;

  switch(_mqtt->connect(_username, _key)) {
    case 0:
      return AIO_CONNECTED;
    case 1:   // invalid mqtt protocol
    case 2:   // client id rejected
    case 4:   // malformed user/pass
    case 5:   // unauthorized
      return AIO_CONNECT_FAILED;
    case 3:   // mqtt service unavailable
    case 6:   // throttled
    case 7:   // banned -> all MQTT bans are temporary, so eventual retry is permitted
      // delay to prevent fast reconnects
      delay(AIO_THROTTLE_RECONNECT_INTERVAL);
      return AIO_DISCONNECTED;
    default:
      return AIO_DISCONNECTED;
  }
}
