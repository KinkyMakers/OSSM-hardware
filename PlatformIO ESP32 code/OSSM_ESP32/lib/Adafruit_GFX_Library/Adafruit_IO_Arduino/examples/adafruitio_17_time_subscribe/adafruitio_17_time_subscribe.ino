// Adafruit IO Time Topic Subscription Example
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Adam Bachman, Brent Rubell for Adafruit Industries
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

// set up the 'time/seconds' topic
AdafruitIO_Time *seconds = io.time(AIO_TIME_SECONDS);

// set up the 'time/milliseconds' topic
AdafruitIO_Time *msecs = io.time(AIO_TIME_MILLIS);

// set up the 'time/ISO-8601' topic
AdafruitIO_Time *iso = io.time(AIO_TIME_ISO);

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO");

  // start MQTT connection to io.adafruit.com
  io.connect();

  // attach message handler for the seconds feed
  seconds->onMessage(handleSecs);

  // attach a message handler for the msecs feed
  msecs->onMessage(handleMillis);

  // attach a message handler for the ISO feed
  iso->onMessage(handleISO);

  // wait for an MQTT connection
  // NOTE: when blending the HTTP and MQTT API, always use the mqttStatus
  // method to check on MQTT connection status specifically
  while(io.mqttStatus() < AIO_CONNECTED) {
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

  // Because this sketch isn't publishing, we don't need
  // a delay() in the main program loop.

}


// message handler for the seconds feed
void handleSecs(char *data, uint16_t len) {
  Serial.print("Seconds Feed: ");
  Serial.println(data);
}


// message handler for the milliseconds feed
void handleMillis(char *data, uint16_t len) {
  Serial.print("Millis Feed: ");
  Serial.println(data);
}


// message handler for the ISO-8601 feed
void handleISO(char *data, uint16_t len) {
  Serial.print("ISO Feed: ");
  Serial.println(data);
}
