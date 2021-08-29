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
#include "SliderBlock.h"

SliderBlock::SliderBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
  min = 0;
  max = 100;
  step = 10;
  label = "Value";
}

SliderBlock::~SliderBlock(){}

String SliderBlock::properties()
{
  String props = "{\"min\":\"";
  props += min;
  props += "\",\"max\":\"";
  props += max;
  props += "\",\"step\":\"";
  props += step;
  props += "\",\"label\":\"";
  props += label;
  props += "\"}";

  return props;
}
