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
#include "ChartBlock.h"

ChartBlock::ChartBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
  historyHours = 0;
  xAxisLabel = "X";
  yAxisLabel = "Y";
  yAxisMin = 0;
  yAxisMax = 100;
}

ChartBlock::~ChartBlock(){}

String ChartBlock::properties()
{

  String props = "{\"historyHours\":\"";
  props += historyHours;
  props += "\",\"xAxisLabel\":\"";
  props += xAxisLabel;
  props += "\",\"yAxisLabel\":\"";
  props += yAxisLabel;
  props += "\",\"yAxisMin\":\"";
  props += yAxisMin;
  props += "\",\"yAxisMax\":\"";
  props += yAxisMax;
  props += "\"}";

  return props;
}
