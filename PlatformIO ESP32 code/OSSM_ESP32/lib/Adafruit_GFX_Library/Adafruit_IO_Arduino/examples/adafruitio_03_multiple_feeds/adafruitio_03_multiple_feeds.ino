// Adafruit IO Multiple Feed Example
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

// holds the current count value for our sketch
int count = 0;
// holds the boolean (true/false) state of the light
bool is_on = false;

// track time of last published messages and limit feed->save events to once
// every IO_LOOP_DELAY milliseconds
#define IO_LOOP_DELAY 15000
unsigned long lastUpdate;

// set up the 'counter' feed
AdafruitIO_Feed *counter = io.feed("counter");

// set up the 'counter-two' feed
AdafruitIO_Feed *counter_two = io.feed("counter-two");

// set up the 'light' feed
AdafruitIO_Feed *light = io.feed("light");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO");

  // connect to io.adafruit.com
  io.connect();

  // attach message handler for the counter feed.
  counter->onMessage(handleCount);

  // attach the same message handler for the second counter feed.
  counter_two->onMessage(handleCount);

  // attach a new message handler for the light feed.
  light->onMessage(handleLight);

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // make sure all feeds get their current values right away
  counter->get();
  counter_two->get();
  light->get();

}

void loop() {

  // process messages and keep connection alive
  io.run();

  if (millis() > (lastUpdate + IO_LOOP_DELAY)) {
    Serial.println();

    // save current count to 'counter'
    Serial.print("sending -> counter ");
    Serial.println(count);
    counter->save(count);

    // increment the count by 1 and save the value to 'counter-two'
    Serial.print("sending -> counter-two ");
    Serial.println(count + 1);
    counter_two->save(count + 1);

    // print out the light value we are sending to Adafruit IO
    Serial.print("sending -> light ");
    if(is_on)
      Serial.println("is on.\n");
    else
      Serial.println("is off.\n");

    // save state of light to 'light' feed
    light->save(is_on);

    // increment count value
    count++;

    // for the purpose of this demo, toggle the
    // light state based on the count value
    if((count % 2) == 0)
      is_on = true;
    else
      is_on = false;

    // update timer
    lastUpdate = millis();
  }

}

// you can set a separate message handler for a single feed,
// as we do in this example for the light feed
void handleLight(AdafruitIO_Data *data) {

  // print out the received light value
  Serial.print("received <- light ");

  // use the isTrue helper to get the
  // boolean state of the light
  if(data->isTrue())
    Serial.println("is on.");
  else
    Serial.println("is off.");

}

// you can also attach multiple feeds to the same
// meesage handler function. both counter and counter-two
// are attached to this callback function, and messages
// for both will be received by this function.
void handleCount(AdafruitIO_Data *data) {

  Serial.print("received <- ");

  // since we are using the same function to handle
  // messages for two feeds, we can use feedName() in
  // order to find out which feed the message came from.
  Serial.print(data->feedName());
  Serial.print(" ");

  // print out the received count or counter-two value
  Serial.println(data->value());

}
