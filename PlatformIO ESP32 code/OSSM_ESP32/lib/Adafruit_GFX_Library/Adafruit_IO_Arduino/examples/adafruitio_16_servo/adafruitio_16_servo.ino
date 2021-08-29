// Adafruit IO Servo Example
// Tutorial Link: https://learn.adafruit.com/adafruit-io-basics-servo
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016-2017 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/

#if defined(ARDUINO_ARCH_ESP32)
  // ESP32Servo Library (https://github.com/madhephaestus/ESP32Servo)
  // installation: library manager -> search -> "ESP32Servo"
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif

// pin used to control the servo
#define SERVO_PIN 2

// create an instance of the servo class
Servo servo;

// set up the 'servo' feed
AdafruitIO_Feed *servo_feed = io.feed("servo");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // tell the servo class which pin we are using
  servo.attach(SERVO_PIN);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up a message handler for the 'servo' feed.
  // the handleMessage function (defined below)
  // will be called whenever a message is
  // received from adafruit io.
  servo_feed->onMessage(handleMessage);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  servo_feed->get();

}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

}

// this function is called whenever a 'servo' message
// is received from Adafruit IO. it was attached to
// the servo feed in the setup() function above.
void handleMessage(AdafruitIO_Data *data) {

  // convert the data to integer
  int angle = data->toInt();

  // make sure we don't exceed the limit
  // of the servo. the range is from 0
  // to 180.
  if(angle < 0)
    angle = 0;
  else if(angle > 180)
    angle = 180;

  servo.write(angle);

}
