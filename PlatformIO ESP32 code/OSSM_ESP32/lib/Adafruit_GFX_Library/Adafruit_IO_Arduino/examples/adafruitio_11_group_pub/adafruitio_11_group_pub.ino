// Adafruit IO Group Publish Example
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

// set up the group
AdafruitIO_Group *group = io.group("example");

int count_1 = 0;
int count_2 = 0;

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

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

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  group->set("count-1", count_1);
  group->set("count-2", count_2);
  group->save();

  Serial.print("sending example.count-1 -> ");
  Serial.println(count_1);
  Serial.print("sending example.count-2 -> ");
  Serial.println(count_2);

  // increment the count_1 by 1
  count_1 += 1;
  // increment the count_2 by 2
  count_2 += 2;

  // wait four seconds (1000 milliseconds == 1 second)
  delay(4000);
}
