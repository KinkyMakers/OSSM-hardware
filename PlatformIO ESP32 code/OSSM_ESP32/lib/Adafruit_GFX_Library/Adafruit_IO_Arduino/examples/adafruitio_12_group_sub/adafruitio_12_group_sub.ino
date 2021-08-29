// Adafruit IO Group Subscribe Example
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2018 Adafruit Industries
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

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  group->onMessage("example.count-1", one);
  group->onMessage("example.count-2", two);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // force IO to update our MQTT subscription with the current values of all feeds
  group->get();
}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

}


// this function is called whenever a 'counter-1' message
// is received from Adafruit IO. it was attached to
// the counter-1 feed in the setup() function above.
void one(AdafruitIO_Data *data) {
  Serial.print("received example.count-1 <- ");
  Serial.println(data->value());
}

// this function is called whenever a 'counter-2' message
// is received from Adafruit IO. it was attached to
// the counter-2 feed in the setup() function above.
void two(AdafruitIO_Data *data) {
  Serial.print("received example.count-2 <- ");
  Serial.println(data->value());
}
