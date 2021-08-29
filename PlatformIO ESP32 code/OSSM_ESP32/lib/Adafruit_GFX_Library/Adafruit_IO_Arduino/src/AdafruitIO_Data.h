//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Copyright (c) 2015-2016 Adafruit Industries
// Authors: Tony DiCola, Todd Treece, Adam Bachman
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.
//
#ifndef ADAFRUITIO_DATA_H
#define ADAFRUITIO_DATA_H

#include "Arduino.h"
#include "AdafruitIO_Definitions.h"

// forward decl
class AdafruitIO_Feed;

class AdafruitIO_Data {

  public:
    AdafruitIO_Data();
    AdafruitIO_Data(AdafruitIO_Feed *f);
    AdafruitIO_Data(AdafruitIO_Feed *f, char *csv);
    AdafruitIO_Data(AdafruitIO_Feed *f, const char *csv);
    AdafruitIO_Data(const char *f);
    AdafruitIO_Data(const char *f, char *csv);

    bool setCSV(char *csv);
    bool setCSV(const char *csv);

    void setLocation(double lat, double lon, double ele=0);

    void setValue(const char *value, double lat=0, double lon=0, double ele=0);
    void setValue(char *value, double lat=0, double lon=0, double ele=0);
    void setValue(bool value, double lat=0, double lon=0, double ele=0);
    void setValue(String value, double lat=0, double lon=0, double ele=0);
    void setValue(int value, double lat=0, double lon=0, double ele=0);
    void setValue(unsigned int value, double lat=0, double lon=0, double ele=0);
    void setValue(long value, double lat=0, double lon=0, double ele=0);
    void setValue(unsigned long value, double lat=0, double lon=0, double ele=0);
    void setValue(float value, double lat=0, double lon=0, double ele=0, int precision=6);
    void setValue(double value, double lat=0, double lon=0, double ele=0, int precision=6);

    char* feedName();

    char* value();
    char* toChar();
    String toString();

    bool toBool();
    bool isTrue();
    bool isFalse();

    int toInt();
    int toPinLevel();
    unsigned int toUnsignedInt();

    double toDouble();
    float toFloat();

    long toLong();
    unsigned long toUnsignedLong();

    int toRed();
    int toGreen();
    int toBlue();
    long toNeoPixel();

    char* toCSV();
    char* charFromDouble(double d, int precision=6);

    double lat();
    double lon();
    double ele();

    AdafruitIO_Data *next_data;

  private:

    char _feed[AIO_FEED_NAME_LENGTH];

    char _csv[AIO_CSV_LENGTH];
    char _value[AIO_DATA_LENGTH];

    double _lat,
           _lon,
           _ele;

    bool _parseCSV();

};

#endif // ADAFRUITIO_DATA_H
