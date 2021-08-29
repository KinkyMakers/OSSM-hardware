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
#include "AdafruitIO_Data.h"
#include "AdafruitIO_Feed.h"

AdafruitIO_Data::AdafruitIO_Data()
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  memset(_value, 0, AIO_DATA_LENGTH);
  memset(_csv, 0, AIO_CSV_LENGTH);
}

AdafruitIO_Data::AdafruitIO_Data(AdafruitIO_Feed *f)
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  strcpy(_feed, f->name);
  memset(_value, 0, AIO_DATA_LENGTH);
  memset(_csv, 0, AIO_CSV_LENGTH);
}

AdafruitIO_Data::AdafruitIO_Data(AdafruitIO_Feed *f, char *csv)
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  strcpy(_feed, f->name);
  memset(_value, 0, AIO_DATA_LENGTH);
  memset(_csv, 0, AIO_CSV_LENGTH);
  strcpy(_csv, csv);

  _parseCSV();
}

AdafruitIO_Data::AdafruitIO_Data(AdafruitIO_Feed *f, const char *csv)
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  strcpy(_feed, f->name);
  memset(_value, 0, AIO_DATA_LENGTH);
  memset(_csv, 0, AIO_CSV_LENGTH);
  strcpy(_csv, csv);

  _parseCSV();
}

AdafruitIO_Data::AdafruitIO_Data(const char *f)
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  strcpy(_feed, f);
  memset(_value, 0, AIO_DATA_LENGTH);
}

AdafruitIO_Data::AdafruitIO_Data(const char *f, char *csv)
{
  _lat = 0;
  _lon = 0;
  _ele = 0;
  next_data = 0;

  memset(_feed, 0, AIO_FEED_NAME_LENGTH);
  strcpy(_feed, f);
  memset(_value, 0, AIO_DATA_LENGTH);
  memset(_csv, 0, AIO_CSV_LENGTH);
  strcpy(_csv, csv);

  _parseCSV();
}

bool AdafruitIO_Data::setCSV(char *csv)
{
  return setCSV((const char *)(csv));
}

bool AdafruitIO_Data::setCSV(const char *csv)
{

  memset(_csv, 0, AIO_CSV_LENGTH);
  strcpy(_csv, csv);
  return _parseCSV();
}

void AdafruitIO_Data::setLocation(double lat, double lon, double ele)
{
  // if lat, lon, ele == 0, don't set them
  if((abs(0-lat) < 0.000001) && (abs(0-lon) < 0.000001) && (abs(0-ele) < 0.000001))
    return;
  _lat = lat;
  _lon = lon;
  _ele = ele;
}

void AdafruitIO_Data::setValue(const char *value, double lat, double lon, double ele)
{
  strcpy(_value, value);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(char *value, double lat, double lon, double ele)
{
  strcpy(_value, value);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(bool value, double lat, double lon, double ele)
{
  if(value)
    strcpy(_value, "1");
  else
    strcpy(_value, "0");

  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(String value, double lat, double lon, double ele)
{
  value.toCharArray(_value, value.length() + 1);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(int value, double lat, double lon, double ele)
{
  memset(_value, 0, AIO_DATA_LENGTH);
  itoa(value, _value, 10);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(unsigned int value, double lat, double lon, double ele)
{
  memset(_value, 0, AIO_DATA_LENGTH);
  utoa(value, _value, 10);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(long value, double lat, double lon, double ele)
{
  memset(_value, 0, AIO_DATA_LENGTH);
  ltoa(value, _value, 10);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(unsigned long value, double lat, double lon, double ele)
{
  memset(_value, 0, AIO_DATA_LENGTH);
  ultoa(value, _value, 10);
  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(float value, double lat, double lon, double ele, int precision)
{
  memset(_value, 0, AIO_DATA_LENGTH);

  #if defined(ARDUINO_ARCH_AVR)
    // Use avrlibc dtostre function on AVR platforms.
    dtostre(value, _value, precision, 0);
  #elif defined(ESP8266)
    // ESP8266 Arduino only implements dtostrf and not dtostre.  Use dtostrf
    // but accept a hint as to how many decimals of precision are desired.
    dtostrf(value, 0, precision, _value);
  #else
    // Otherwise fall back to snprintf on other platforms.
    snprintf(_value, sizeof(_value) - 1, "%0.*f", precision, value);
  #endif

  setLocation(lat, lon, ele);
}

void AdafruitIO_Data::setValue(double value, double lat, double lon, double ele, int precision)
{
  memset(_value, 0, AIO_DATA_LENGTH);

  #if defined(ARDUINO_ARCH_AVR)
    // Use avrlibc dtostre function on AVR platforms.
    dtostre(value, _value, precision, 0);
  #elif defined(ESP8266)
    // ESP8266 Arduino only implements dtostrf and not dtostre.  Use dtostrf
    // but accept a hint as to how many decimals of precision are desired.
    dtostrf(value, 0, precision, _value);
  #else
    // Otherwise fall back to snprintf on other platforms.
    snprintf(_value, sizeof(_value) - 1, "%0.*f", precision, value);
  #endif

  setLocation(lat, lon, ele);
}

char* AdafruitIO_Data::feedName()
{
  if(! _feed)
    return (char*)"";

  return _feed;
}

char* AdafruitIO_Data::value()
{
  return toChar();
}

char* AdafruitIO_Data::toChar()
{
  return _value;
}

String AdafruitIO_Data::toString()
{
  if(! _value)
    return String();

  return String(_value);
}

bool AdafruitIO_Data::toBool()
{
  if(! _value)
    return false;

  if(strcmp(_value, "1") == 0 || _value[0] == 't' || _value[0] == 'T')
    return true;
  else
    return false;
}

bool AdafruitIO_Data::isTrue()
{
  return toBool();
}

bool AdafruitIO_Data::isFalse()
{
  return !toBool();
}

int AdafruitIO_Data::toInt()
{
  if(! _value)
    return 0;

  char* endptr;
  return (int)strtol(_value, &endptr, 10);
}

int AdafruitIO_Data::toPinLevel()
{
  if(isTrue())
    return HIGH;
  else
    return LOW;
}

unsigned int AdafruitIO_Data::toUnsignedInt()
{
  if(! _value)
    return 0;

  char* endptr;
  #ifdef ESP8266
    // For some reason strtoul is not defined on the ESP8266 platform right now.
    // Just use a strtol function and hope for the best.
    return (unsigned int)strtol(_value, &endptr, 10);
  #else
    return (unsigned int)strtoul(_value, &endptr, 10);
  #endif
}

float AdafruitIO_Data::toFloat()
{
  if(! _value)
    return 0;

  char* endptr;
  return (float)strtod(_value, &endptr);
}

double AdafruitIO_Data::toDouble()
{
  if(! _value)
    return 0;

  char* endptr;
  return strtod(_value, &endptr);
}

long AdafruitIO_Data::toLong()
{
  if(! _value)
    return 0;

  char* endptr;
  return strtol(_value, &endptr, 10);
}

unsigned long AdafruitIO_Data::toUnsignedLong()
{
  if(! _value)
    return 0;

  char* endptr;
  #ifdef ESP8266
      // For some reason strtoul is not defined on the ESP8266 platform right now.
      // Just use a strtol function and hope for the best.
      return (unsigned long)strtol(_value, &endptr, 10);
  #else
      return strtoul(_value, &endptr, 10);
  #endif
}

int AdafruitIO_Data::toRed()
{
    // Convert 0xRRGGBB to red.
    if (! _value)
    {
        return 0;
    }
    char r[5];
    strcpy(r, "0x");
    strncpy(&r[2], toChar() + 1, 2);
    r[4] = '\x00';
    return (int)strtol(r, NULL, 0);
}

int AdafruitIO_Data::toGreen()
{
    // Convert 0xRRGGBB to green.
    if (! _value)
    {
        return 0;
    }
    char g[5];
    strcpy(g, "0x");
    strncpy(&g[2], toChar() + 3, 2);
    g[4] = '\x00';
    return (int)strtol(g, NULL, 0);
}

int AdafruitIO_Data::toBlue()
{
    // Convert 0xRRGGBB to blue.
    if (! _value)
    {
        return 0;
    }
    char b[5];
    strcpy(b, "0x");
    strncpy(&b[2], toChar() + 5, 2);
    b[4] = '\x00';
    return (int)strtol(b, NULL, 0);
}

long AdafruitIO_Data::toNeoPixel()
{
    if (! _value)
    {
        return 0;
    }
    char rgb[9];
    strcpy(rgb, "0x");
    strncpy(&rgb[2], toChar() + 1, 6);
    return strtol(rgb, NULL, 0);
}

char* AdafruitIO_Data::toCSV()
{
  if(! _value)
    return _csv;

  memset(_csv, 0, AIO_CSV_LENGTH);

  strcpy(_csv, "\"");
  strcat(_csv, _value);
  strcat(_csv, "\",");
  strcat(_csv, charFromDouble(_lat));
  strcat(_csv, ",");
  strcat(_csv, charFromDouble(_lon));
  strcat(_csv, ",");
  strcat(_csv, charFromDouble(_ele, 2));

  return _csv;
}

double AdafruitIO_Data::lat()
{
  return _lat;
}

double AdafruitIO_Data::lon()
{
  return _lon;
}

double AdafruitIO_Data::ele()
{
  return _ele;
}

static char _double_buffer[20];

char* AdafruitIO_Data::charFromDouble(double d, int precision)
{
  memset(_double_buffer, 0, sizeof(_double_buffer));

  #if defined(ARDUINO_ARCH_AVR)
    // Use avrlibc dtostre function on AVR platforms.
    dtostre(d, _double_buffer, 10, 0);
  #elif defined(ESP8266)
    // ESP8266 Arduino only implements dtostrf and not dtostre.  Use dtostrf
    // but accept a hint as to how many decimals of precision are desired.
    dtostrf(d, 0, precision, _double_buffer);
  #else
    // Otherwise fall back to snprintf on other platforms.
    snprintf(_double_buffer, sizeof(_double_buffer)-1, "%f", d);
  #endif

  return _double_buffer;
}

/*
 * From the csv_parser project by semitrivial
 * https://github.com/semitrivial/csv_parser/blob/93246cac509f85da12c6bb8c641fa75cd863c34f/csv.c - retrieved 2017-11-09
 *
 * MIT License
 *
 * Copyright 2016 Samuel Alexander
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

static int count_fields( const char *line )
{
  const char *ptr;
  int cnt, fQuote;

  for ( cnt = 1, fQuote = 0, ptr = line; *ptr; ptr++ )
  {
    if ( fQuote )
    {
      if ( *ptr == '\"' )
      {
        if ( ptr[1] == '\"' )
        {
          ptr++;
          continue;
        }
        fQuote = 0;
      }
      continue;
    }

    switch( *ptr )
    {
      case '\"':
        fQuote = 1;
        continue;
      case ',':
        cnt++;
        continue;
      default:
        continue;
    }
  }

  if ( fQuote )
  {
    return -1;
  }

  return cnt;
}

/*
 *  Given a string containing no linebreaks, or containing line breaks
 *  which are escaped by "double quotes", extract a NULL-terminated
 *  array of strings, one for every cell in the row.
 */
char **parse_csv( const char *line )
{
  char **buf, **bptr, *tmp, *tptr;
  const char *ptr;
  int fieldcnt, fQuote, fEnd;

  fieldcnt = count_fields( line );

  if ( fieldcnt == -1 )
  {
    return NULL;
  }

  buf = (char **)malloc( sizeof(char*) * (fieldcnt+1) );

  if ( !buf )
  {
    return NULL;
  }

  tmp = (char *)malloc( strlen(line) + 1 );

  if ( !tmp )
  {
    free( buf );
    return NULL;
  }

  bptr = buf;

  for ( ptr = line, fQuote = 0, *tmp = '\0', tptr = tmp, fEnd = 0; ; ptr++ )
  {
    if ( fQuote )
    {
      if ( !*ptr )
      {
        break;
      }

      if ( *ptr == '\"' )
      {
        if ( ptr[1] == '\"' )
        {
          *tptr++ = '\"';
          ptr++;
          continue;
        }
        fQuote = 0;
      }
      else {
        *tptr++ = *ptr;
      }

      continue;
    }

    switch( *ptr )
    {
      case '\"':
        fQuote = 1;
        continue;
      case '\0':
        fEnd = 1;
      case ',':
        *tptr = '\0';
        *bptr = strdup( tmp );

        if ( !*bptr )
        {
          for ( bptr--; bptr >= buf; bptr-- )
          {
            free( *bptr );
          }
          free( buf );
          free( tmp );

          return NULL;
        }

        bptr++;
        tptr = tmp;

        if ( fEnd )
        {
          break;
        } else
        {
          continue;
        }

      default:
        *tptr++ = *ptr;
        continue;
    }

    if ( fEnd )
    {
      break;
    }
  }

  *bptr = NULL;
  free( tmp );
  return buf;
}

//// END simple_csv SECTION

bool AdafruitIO_Data::_parseCSV()
{

  int field_count = count_fields(_csv);

  if (field_count > 0)
  {
    // this is normal IO data in `value,lat,lon,ele` format
    char **fields = parse_csv(_csv);

    // first field is handled as string
    strcpy(_value, fields[0]);
    field_count--;

    // locations fields are handled with char * to float conversion
    if (field_count > 0)
    {
      _lat = atof(fields[1]);
      field_count--;
    }

    if (field_count > 0)
    {
      _lon = atof(fields[1]);
      field_count--;
    }

    if (field_count > 0)
    {
      _ele = atof(fields[1]);
      field_count--;
    }

    // cleanup to avoid leaks
    int i = 0;
    while (fields[i] != NULL){
      free(fields[i++]);
    }
    free(fields);

    return field_count == 0;
  }
  else
  {
    return false;
  }

  return true;
}

