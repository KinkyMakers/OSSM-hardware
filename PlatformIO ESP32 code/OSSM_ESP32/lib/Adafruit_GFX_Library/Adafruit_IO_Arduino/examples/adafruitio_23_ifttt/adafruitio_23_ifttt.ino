// Adafruit IO IFTTT Example - Gmailbox
// Tutorial Link: https://learn.adafruit.com/gmailbox
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

// Import Servo Libraries
#if defined(ARDUINO_ARCH_ESP32)
  // ESP32Servo Library (https://github.com/madhephaestus/ESP32Servo)
  // installation: library manager -> search -> "ESP32Servo"
  #include <ESP32Servo.h>
#else
  #include <Servo.h>
#endif

/************************ Example Starts Here *******************************/

// pin used to control the servo
#define SERVO_PIN 14

// Flag's up position, in degrees
#define FLAG_UP 0

// Flag's down position, in degrees
#define FLAG_DOWN 180

// How long to hold the flag up, in seconds
#define FLAG_DELAY 2

// create an instance of the servo class
Servo servo;

// set up the 'servo' feed
AdafruitIO_Feed *gmail_feed = io.feed("gmail");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("IFTTT Gmailbox");

  // tell the servo class which pin we are using
  servo.attach(SERVO_PIN);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up a message handler for the 'servo' feed.
  // the handleMessage function (defined below)
  // will be called whenever a message is
  // received from adafruit io.
  gmail_feed->onMessage(handleMessage);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
  gmail_feed->get();

  // write flag to down position
  servo.write(FLAG_DOWN);

}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

}

// this function is called whenever a 'gmail' message
// is received from Adafruit IO. it was attached to
// the gmail feed in the setup() function above.
void handleMessage(AdafruitIO_Data *data) {

  Serial.println("You've got mail!");
  servo.write(FLAG_UP);
  // wait FLAG_DELAY seconds
  delay(FLAG_DELAY * 1000);
  servo.write(FLAG_DOWN);
}
