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
#include "ToggleBlock.h"

ToggleBlock::ToggleBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
  onText = "1";
  offText = "0";
}

ToggleBlock::~ToggleBlock(){}

String ToggleBlock::properties()
{
  String props = "{\"onText\":\"";
  props += onText;
  props += "\",\"offText\":\"";
  props += offText;
  props += "\"}";

  return props;
}
