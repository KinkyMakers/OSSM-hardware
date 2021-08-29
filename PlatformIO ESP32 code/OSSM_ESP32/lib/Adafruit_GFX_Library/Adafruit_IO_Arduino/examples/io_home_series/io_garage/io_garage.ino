// Adafruit IO Garage
// Tutorial Link: https://learn.adafruit.com/adafruit-io-home-garage
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

// include the servo library
#include <Servo.h> 

// include the ultrasonic library 
#include <Ultrasonic.h>

/************************ Example Starts Here *******************************/

#define SERVOPIN        15
#define DOORPIN          0

// servo (garage door opener) angle presets
#define DOORCLOSE_POS   30
#define DOOROPEN_POS   130

// sketch starts assuming the the door is closed
int doorState = LOW;

// ultrasonic sensor distance
int ultrasonicRead;

// set up the ultrasonic object (trigger, echo)
Ultrasonic ultrasonic(12, 13);
// create servo object
Servo doorServo;

// set up the Adafruit IO Feeds
AdafruitIO_Feed *openGarage_feed = io.feed("garage-opener");
AdafruitIO_Feed *closeGarage_feed = io.feed("garage-closer");
AdafruitIO_Feed *carDistance_feed = io.feed("carDistance");
AdafruitIO_Feed *doorStatus_feed = io.feed("doorStatus");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // set up message handlers
  openGarage_feed->onMessage(handleGarageOpener);
  closeGarage_feed->onMessage(handleGarageCloser);


  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // attach the servo object to pin 16
  doorServo.attach(SERVOPIN);
  // start with the door closed
  doorServo.write(DOORCLOSE_POS);
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();
  
  // read the door sensor
  readDoorSensor();
  
  // read the ultrasonic sensor
  ultrasonicRead = ultrasonic.distanceRead(INC);
  delay(1000);
  Serial.print("Distance from car (inches):"); Serial.println(ultrasonicRead);
  carDistance_feed->save(ultrasonicRead);
}

// reads the status of the garage door and sends to adafruit io
void readDoorSensor() {
  doorState = digitalRead(DOORPIN);
  if (doorState == LOW) {
    Serial.println("* Door Closed");
    doorStatus_feed->save(0);
  } else {
    Serial.println("* Door Open");
    doorStatus_feed->save(1);
  }
  delay(2000);
}

// 'open garage' button handler
void handleGarageOpener(AdafruitIO_Data *data) {
    Serial.println("Door Opened!");
    doorServo.write(DOOROPEN_POS);
}

// 'close garage' button handler
void handleGarageCloser(AdafruitIO_Data *data) {
     Serial.println("Door Closed!");
    doorServo.write(DOORCLOSE_POS);
}


