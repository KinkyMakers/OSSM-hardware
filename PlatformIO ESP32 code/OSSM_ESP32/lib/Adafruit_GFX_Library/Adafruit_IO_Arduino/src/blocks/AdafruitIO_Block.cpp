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
#include "AdafruitIO_Block.h"
#include "AdafruitIO.h"
#include "AdafruitIO_Dashboard.h"

AdafruitIO_Block::AdafruitIO_Block(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f)
{
  _dashboard = d;
  _feed = f;
}

AdafruitIO_Block::~AdafruitIO_Block(){}

String AdafruitIO_Block::properties()
{
  String props = "{}";
  return props;
}

String AdafruitIO_Block::dimensions()
{
  String dim = "\",\"size_x\":\"";
  dim += _width();
  dim += "\",\"size_y\":\"";
  dim += _height();

  if(_row() > 0) {
    dim += "\",\"row\":\"";
    dim += _row();
  }

  if(_column() > 0) {
    dim += "\",\"column\":\"";
    dim += _column();
  }

  return dim;
}

const char* AdafruitIO_Block::type()
{
  return _visual_type;
}

bool AdafruitIO_Block::save()
{
  HttpClient *http = _dashboard->io()->_http;

  String url = "/api/v2/";
  url += _dashboard->user();
  url += "/dashboards/";
  url += _dashboard->name;
  url += "/blocks";

  String block_feeds = "[{\"feed_id\":\"";
  block_feeds += _feed->name;
  block_feeds += "\"}]";

  String body = "{\"visual_type\":\"";
  body += type();
  body += dimensions();
  body += "\",\"properties\":";
  body += properties();
  body += ",\"block_feeds\":";
  body += block_feeds;
  body += "}";

  http->beginRequest();
  http->post(url.c_str());

  http->sendHeader("Content-Type", "application/json");
  http->sendHeader("Content-Length", body.length());
  http->sendHeader("X-AIO-Key", _dashboard->io()->_key);

  // the following call to endRequest
  // should be replaced by beginBody once the
  // Arduino HTTP Client Library is updated
  // http->beginBody();
  http->endRequest();

  http->print(body);
  http->endRequest();

  int status = http->responseStatusCode();
  http->responseBody(); // needs to be read even if not used

  return status == 200;
}
