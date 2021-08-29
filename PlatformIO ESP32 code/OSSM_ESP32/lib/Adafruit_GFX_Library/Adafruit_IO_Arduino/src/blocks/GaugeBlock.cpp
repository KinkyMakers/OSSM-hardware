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
#include "GaugeBlock.h"

GaugeBlock::GaugeBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
  min = 0;
  max = 100;
  ringWidth = "thin";
  label = "Value";
}

GaugeBlock::~GaugeBlock(){}

String GaugeBlock::properties()
{
  int w = 0;
  
  if (strcmp(ringWidth, "thin")) {
    w = 25;
  } else {
    w = 50;
  }

  String props = "{\"minValue\":\"";
  props += min;
  props += "\",\"maxValue\":\"";
  props += max;
  props += "\",\"ringWidth\":\"";
  props += w;
  props += "\",\"label\":\"";
  props += label;
  props += "\"}";

  return props;
}
