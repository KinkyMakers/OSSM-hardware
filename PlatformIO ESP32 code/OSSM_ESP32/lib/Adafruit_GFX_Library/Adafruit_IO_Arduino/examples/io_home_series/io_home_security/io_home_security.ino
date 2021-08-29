// Adafruit IO House: Security System
//
// Learn Guide: https://learn.adafruit.com/adafruit-io-home-security
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

// include the NeoPixel library
#include "Adafruit_NeoPixel.h"

// include the SGP30 library
#include <Wire.h>
#include "Adafruit_SGP30.h"

/************************ Example Starts Here *******************************/

// delay the main `io.run()` loop 
#define LOOP_DELAY 3000
// delay for each sensor send to adafruit io
#define SENSOR_DELAY 1000

// PIR sensor input pin
#define pirPin 13
// reed switch input pin
#define doorPin 2
// piezo (alarm) buzzer
#define piezoPin 14

/**** Time Setup ****/
// set the hour to start at
int startingHour = 1;
// set the second to start at
int seconds = 56;
// set the minutes to start at
int minutes = 56;
// set the hour at which the motion alarm should trigger
int alarmHour = 16;

unsigned long currentTime = 0;
unsigned long prevTime = 0;
int currentHour = startingHour;


/*********NeoPixel Setup*********/
// pin the NeoPixel strip and jewel are connected to
#define NEOPIXEL_PIN         12
// amount of neopixels on the NeoPixel strip
#define STRIP_PIXEL_COUNT     60
#define JEWEL_PIXEL_COUNT     7
// type of neopixels used by the NeoPixel strip and jewel.
#define PIXEL_TYPE    NEO_GRB + NEO_KHZ800
// init. neoPixel Strip
Adafruit_NeoPixel strip = Adafruit_NeoPixel(STRIP_PIXEL_COUNT, NEOPIXEL_PIN, PIXEL_TYPE);

// sketch starts assuming no motion is detected
int pirState = LOW;
// sketch starts assuming the the door is closed
int doorState = LOW;
// alarm state
bool isAlarm = false;
// variable for reading the pin status
int pirRead = 0;
// SGP30 Sensor Object
Adafruit_SGP30 sgp;

/*** Adafruit IO Feed Setup ***/
// 'indoor-lights' feed
AdafruitIO_Feed *indoorLights = io.feed("indoor-lights");
// `outdoor-lights` feed
AdafruitIO_Feed *outdoorLights = io.feed("outdoor-lights");
// `front-door` feed
AdafruitIO_Feed *frontDoor = io.feed("front-door");
// `motion-detector` feed
AdafruitIO_Feed *motionFeed = io.feed("motion-detector"); 
// `home-alarm` feed
AdafruitIO_Feed *alarm = io.feed("home-alarm");
// 'tvoc' feed
AdafruitIO_Feed *tvocFeed = io.feed("tvoc");
// 'eco2' feed
AdafruitIO_Feed *eco2Feed = io.feed("eco2");

void setup() {
  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);
  Serial.println("Adafruit IO Home: Security");
  
  Serial.println("Connecting to Adafruit IO");
  // start MQTT connection to io.adafruit.com
  io.connect();
  
  // attach a message handler for the `home-alarm` feed
  alarm->onMessage(handleAlarm);
  // subscribe to lighting feeds and register message handlers
  indoorLights->onMessage(indoorLightHandler);
  outdoorLights->onMessage(outdoorLightHandler);

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
  
  // declare PIR sensor as input
  pinMode(pirPin, INPUT);
  // declare reed switch as input
  pinMode(doorPin, INPUT);
  // set up the SGP30 sensor
  setupSGP30();
  // init the neopixel strip and set to `off`
  strip.begin();
  strip.show();
}
 
void loop(){
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  getTime();
  Serial.println("* read door sensor...");
  readDoorSensor();
  Serial.println("* read motion detector");
  readPIR();
  Serial.println("* reading SGP30...");
  readSGP30();

  // check if the alarm toggle is armed from the dashboard
  if (isAlarm == true) {
    if (doorState == HIGH || (currentHour>alarmHour && pirState == HIGH)) {
      playAlarmAnimation();
   }
  }
}

void playAlarmAnimation() {
// plays the alarm piezo buzzer and turn on/off neopixels  
    Serial.println("ALARM TRIGGERED!");
    tone(piezoPin, 220, 2);
    for(int i=0; i<JEWEL_PIXEL_COUNT; ++i) {
      strip.setPixelColor(i, 255, 0, 0);
    }
    strip.show();
    delay(500);
    for(int i=0; i<JEWEL_PIXEL_COUNT; ++i) {
      strip.setPixelColor(i, 0, 0, 0);
    }
    strip.show();
}

void readDoorSensor() {
// reads the status of the front door and sends to adafruit io
  doorState = digitalRead(doorPin);
  if (doorState == LOW) {
    Serial.println("* Door Closed");
    frontDoor->save(1);
  } else {
    Serial.println("* Door Open");
    frontDoor->save(3);
  }
  delay(SENSOR_DELAY);
}

void readPIR() {
  // check if motion is detected in front of the home
  pirRead = digitalRead(pirPin);
  if (pirRead == HIGH) {
    if (pirState == LOW) {
      // we have just turned on
      Serial.println("* Motion detected in front of home!");
      motionFeed->save(3);
      pirState = HIGH;
    }
  } 
  else {
    if (pirState == HIGH) {
      Serial.println("* Motion ended.");
      motionFeed->save(0);
      pirState = LOW;
    }
  }
  delay(SENSOR_DELAY);
}

void readSGP30() {
// reads the SGP30 sensor and sends data to Adafruit IO
  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.println(" ppm");
  tvocFeed->save(int(sgp.TVOC));
  delay(SENSOR_DELAY/2);
  eco2Feed->save(int(sgp.eCO2));
  delay(SENSOR_DELAY/2);
}

/*** MQTT messageHandlers ***/
void handleAlarm(AdafruitIO_Data *data) {
// handle the alarm toggle on the Adafruit IO Dashboard
  String toggleValue = data->toString();
  Serial.print("> rcv alarm: ");
  Serial.println(toggleValue);
  if(toggleValue == String("ON")) {
    Serial.println("* Alarm Set: ON");
    isAlarm = true;
  } else {
    Serial.println("* Alarm Set: OFF");
    isAlarm = false;
  }
}

// handles the indoor light colorpicker on the Adafruit IO Dashboard
void indoorLightHandler(AdafruitIO_Data *data) {
  Serial.print("-> indoor light HEX: ");
  Serial.println(data->value());
  long color = data->toNeoPixel();
  // set the color of each NeoPixel in the jewel
  for(int i=0; i<JEWEL_PIXEL_COUNT; ++i) {
    strip.setPixelColor(i, color);
  }
  // 'set' the neopixel jewel to the new color
  strip.show();
}

// handles the outdoor light colorpicker on the Adafruit IO Dashboard
void outdoorLightHandler(AdafruitIO_Data *data) {
  Serial.print("-> outdoor light HEX: ");
  Serial.println(data->value());
  long color = data->toNeoPixel();
   // set the color of each NeoPixel in the strip
  for(int i=JEWEL_PIXEL_COUNT; i<STRIP_PIXEL_COUNT+JEWEL_PIXEL_COUNT; ++i) {
    strip.setPixelColor(i, color);
  }
  // 'set' the neopixel strip to the new color
  strip.show();
}


void setupSGP30(){
// sets up the SGP30 Sensor
  if (! sgp.begin()) {
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);
}

void getTime() {
  currentTime = millis()/1000;
  seconds = currentTime - prevTime;
  
  if (seconds == 60) {
    prevTime = currentTime;
    minutes += 1;
  }
  if (minutes == 60) { 
    minutes = 0;
    currentHour += 1;
  }
  if (currentHour == 24) {
    currentHour = 0;
  }
  
  Serial.print("Time:");
  Serial.print(currentHour);
  Serial.print(":");
  Serial.print(minutes);
  Serial.print(":"); 
  Serial.println(seconds); 
}
