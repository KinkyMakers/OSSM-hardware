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
#include "StreamBlock.h"

StreamBlock::StreamBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f) : AdafruitIO_Block(d, f)
{
    fontSize = "small";
    fontColor = "green";
    showErrors = true;
    showTimestamp = true;
    showName = true;
}

StreamBlock::~StreamBlock() {}

String StreamBlock::properties()
{
    int s = 0;

    if (strcmp(fontSize, "small") == 0) {
        s = 12;
    }
    if (strcmp(fontSize, "medium") == 0) {
        s = 18;
    }
    else {
        s = 24;
    }

    String props = "{\"fontSize\":\"";
    props += s;
    props += "\",\"fontColor\":\"";
    props += (strcmp(fontColor, "white") == 0) ? "#ffffff" : "#63de00";
    props += "\",\"errors\":\"";
    props += showErrors ? "yes" : "no";
    props += "\",\"showTimestamp\":\"";
    props += showTimestamp ? "yes" : "no";
    props += "\",\"showName\":\"";
    props += showName ? "yes" : "no";
    props += "\"}";

    return props;
}
