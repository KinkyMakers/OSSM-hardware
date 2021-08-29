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
#include "MapBlock.h"

MapBlock::MapBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
    historyHours = 0;
    tile = "contrast";
}

MapBlock::~MapBlock() {}

String MapBlock::properties()
{

    if ((strcmp(tile, "contrast") != 0) && (strcmp(tile, "street") != 0) && (strcmp(tile, "sat") != 0))
    {
        tile = "contrast";
    }

    props = "{\"historyHours\":\"";
    props += historyHours;
    props += "\",\"tile\":\"";
    props += tile;
    props += "\"}";

    return props;
}
