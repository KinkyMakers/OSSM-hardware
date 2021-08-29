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
#ifndef ADAFRUITIO_MOMENTARYBLOCK_H
#define ADAFRUITIO_MOMENTARYBLOCK_H

#include "AdafruitIO_Block.h"

class MomentaryBlock : public AdafruitIO_Block {

  public:
    MomentaryBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f);
    ~MomentaryBlock();

    const char *text;
    const char *value;
    const char *release;

    int width = 2;
    int height = 2;

    String properties();
    const char* type() { return _visual_type; }

  protected:
    const char *_visual_type = "momentary_button";

    int _width() { return width; }
    int _height() { return height; }
    int _row() { return row; }
    int _column() { return column; }


};

#endif // ADAFRUITIO_MOMENTARYBLOCK_H
