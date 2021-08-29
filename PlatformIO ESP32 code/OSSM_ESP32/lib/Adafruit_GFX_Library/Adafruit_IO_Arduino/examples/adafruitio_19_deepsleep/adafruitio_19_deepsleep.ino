// Adafruit IO DeepSleep Example (HUZZAH8266)
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Brent Rubell for Adafruit Industries
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
#define DEEPSLEEP_DURATION 20e6

void setup() {
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while (!Serial);
  Serial.println("Adafruit IO + DeepSleep");
  
  // connect to the Adafruit IO Library 
  connectAIO();

  // set up and write to deepsleep feed
  feedWrite();

  // let's go back to sleep for DEEPSLEEP_DURATION seconds...
  Serial.println("sleeping...");
  // Put the Huzzah into deepsleep for DEEPSLEEP_DURATION
  // NOTE: Make sure Pin 16 is connected to RST
  ESP.deepSleep(1000000 * 2);
}

// NOOP
void loop() {
}


void feedWrite(){
   // set up `deepsleep` feed
  AdafruitIO_Feed *deepsleep = io.feed("deepsleep");
  Serial.println("sending value to feed 'deepsleep");
  // send data to deepsleep feed
  deepsleep->save(1);
  // write data to AIO
  io.run();
}
void connectAIO() {
  Serial.println("Connecting to Adafruit IO...");
  io.connect();

  // wait for a connection
  while (io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
}
