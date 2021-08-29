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
#ifndef ADAFRUITIO_TOGGLEBLOCK_H
#define ADAFRUITIO_TOGGLEBLOCK_H

#include "AdafruitIO_Block.h"

class ToggleBlock : public AdafruitIO_Block {

  public:
    ToggleBlock(AdafruitIO_Dashboard *d, AdafruitIO_Feed *f);
    ~ToggleBlock();

    const char *onText;
    const char *offText;

    int width = 4;
    int height = 2;

    String properties();
    const char* type() { return _visual_type; }

  protected:
    const char *_visual_type = "toggle_button";

    int _width() { return width; }
    int _height() { return height; }
    int _row() { return row; }
    int _column() { return column; }

};

#endif // ADAFRUITIO_TOGGLEBLOCK_H
