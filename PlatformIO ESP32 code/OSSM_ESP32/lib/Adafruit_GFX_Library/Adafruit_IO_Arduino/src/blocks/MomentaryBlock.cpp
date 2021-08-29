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
#include "MomentaryBlock.h"

MomentaryBlock::MomentaryBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
  text = "RESET";
  value = "1";
  release = "0";
}

MomentaryBlock::~MomentaryBlock(){}

String MomentaryBlock::properties()
{
  String props = "{\"text\":\"";
  props += text;
  props += "\",\"value\":\"";
  props += value;
  props += "\",\"release\":\"";
  props += release;
  props += "\"}";

  return props;
}
