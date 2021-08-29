// Adafruit IO Type Conversion Example
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/

// set up variables for  all of the supported types
char *char_val = "character value";
String string_val = "string value";
bool bool_val = true;
int int_val = -10;
unsigned int uint_val = 20;
long long_val = 186000L;
unsigned long ulong_val = millis();
double double_val = 1.3053;
float float_val = 2.4901;

// set up variable that will allow us to flip between sending types
int current_type = 0;

// track time of last published messages and limit feed->save events to once
// every IO_LOOP_DELAY milliseconds
#define IO_LOOP_DELAY 5000
unsigned long lastUpdate;

// set up the 'type' feed
AdafruitIO_Feed *type = io.feed("type");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO");

  // connect to io.adafruit.com
  io.connect();

  // set up a message handler for the type feed.
  type->onMessage(handleMessage);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

}

void loop() {

  // process messages and keep connection alive
  io.run();

  if (millis() > (lastUpdate + IO_LOOP_DELAY)) {
    Serial.println("----- sending -----");

    // in order to demonstrate sending values
    // as different types, we will switch between
    // types in a loop using the current_type variable
    if(current_type == 0) {
      Serial.print("char: ");
      Serial.println(char_val);
      type->save(char_val);
    } else if(current_type == 1) {
      Serial.print("string: ");
      Serial.println(string_val);
      type->save(string_val);
    } else if(current_type == 2) {
      Serial.print("bool: ");
      Serial.println(bool_val);
      type->save(bool_val);
    } else if(current_type == 3) {
      Serial.print("int: ");
      Serial.println(int_val);
      type->save(int_val);
    } else if(current_type == 4) {
      Serial.print("unsigned int: ");
      Serial.println(uint_val);
      type->save(uint_val);
    } else if(current_type == 5) {
      Serial.print("long: ");
      Serial.println(long_val);
      type->save(long_val);
    } else if(current_type == 6) {
      Serial.print("unsigned long: ");
      Serial.println(ulong_val);
      type->save(ulong_val);
    } else if(current_type == 7) {
      Serial.print("double: ");
      Serial.println(double_val);
      type->save(double_val);
    } else if(current_type == 8) {
      Serial.print("float: ");
      Serial.println(float_val);
      type->save(float_val);
    }

    // move to the next type
    current_type++;

    // reset type if we have reached the end
    if(current_type > 8)
      current_type = 0;

    Serial.println();

    lastUpdate = millis();
  }

}

// this function will demonstrate how to convert
// the received data to each available type.
void handleMessage(AdafruitIO_Data *data) {

  // print out the received count value
  Serial.println("----- received -----");

  // value() returns char*
  Serial.print("value: ");
  Serial.println(data->value());

  // get char* value
  Serial.print("toChar: ");
  Serial.println(data->toChar());

  // get String value
  Serial.print("toString: ");
  Serial.println(data->toString());

  // get double value
  Serial.print("toDouble: ");
  Serial.println(data->toDouble(), 6);

  // get double value
  Serial.print("toFloat: ");
  Serial.println(data->toFloat(), 6);

  // get int value
  Serial.print("toInt: ");
  Serial.println(data->toInt());

  // get unsigned int value
  Serial.print("toUnsignedInt: ");
  Serial.println(data->toUnsignedInt());

  // get long value
  Serial.print("toLong: ");
  Serial.println(data->toLong());

  // get unsigned long value
  Serial.print("toUnsignedLong: ");
  Serial.println(data->toUnsignedLong());

  // get bool value
  Serial.print("toBool: ");
  Serial.println(data->toBool());

  // get isTrue (bool) value
  Serial.print("isTrue: ");
  Serial.println(data->isTrue());

  // get isFalse (bool) value
  Serial.print("isFalse: ");
  Serial.println(data->isFalse());

  Serial.println();

}
