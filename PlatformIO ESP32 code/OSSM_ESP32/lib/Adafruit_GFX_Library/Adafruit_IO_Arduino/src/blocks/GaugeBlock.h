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
#ifndef ADAFRUITIO_GAUGEBLOCK_H
#define ADAFRUITIO_GAUGEBLOCK_H

#include "AdafruitIO_Block.h"

class GaugeBlock : public AdafruitIO_Block {

  public:
    GaugeBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f);
    ~GaugeBlock();

    int min;
    int max;

    const char *ringWidth;
    const char *label;

    int width = 4;
    int height = 4;

    String properties();
    const char* type() { return _visual_type; }
  protected:
    const char *_visual_type = "gauge";

    int _width() { return width; }
    int _height() { return height; }
    int _row() { return row; }
    int _column() { return column; }

};

#endif // ADAFRUITIO_GAUGEBLOCK_H
