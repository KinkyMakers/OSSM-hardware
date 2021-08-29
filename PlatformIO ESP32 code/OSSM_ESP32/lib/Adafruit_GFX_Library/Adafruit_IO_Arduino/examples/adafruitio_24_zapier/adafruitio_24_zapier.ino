// Adafruit IO Time Tracking Cube
// Tutorial Link: https://learn.adafruit.com/time-tracking-cube
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Brent Rubell for Adafruit Industries
// Copyright (c) 2019 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/
#include <Wire.h>
#include <Adafruit_LIS3DH.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_NeoPixel.h>

// Prop-Maker Wing
#define NEOPIXEL_PIN 2
#define POWER_PIN 15

// Used for Pizeo
#define PIEZO_PIN 0

// # of Pixels Attached
#define NUM_PIXELS 8

// Adafruit_LIS3DH Setup
Adafruit_LIS3DH lis = Adafruit_LIS3DH();

// NeoPixel Setup
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// Set up the 'cubeTask' feed
AdafruitIO_Feed *cubetask = io.feed("cubetask");

/* Time Tracking Cube States
 * 1: Cube Tilted Left
 * 2: Cube Tilted Right
 * 3: Cube Neutral, Top
*/
int cubeState = 0;

// Previous cube orientation state
int prvCubeState = 0;

// Tasks (change these to what you're currently working on)
String taskOne = "Write Learn Guide";
String taskTwo = "Write Code";

// Adafruit IO sending delay, in seconds
int sendDelay = 0.5;

// Time-Keeping
unsigned long currentTime;
unsigned long prevTime;
int seconds = 0;
int minutes = 0;

void setup()
{
  // start the serial connection
  Serial.begin(9600);
  // wait for serial monitor to open
  while (!Serial)
    ;
  Serial.println("Adafruit IO Time Tracking Cube");

  // disabling low-power mode on the prop-maker wing
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);

  // Initialize LIS3DH
  if (!lis.begin(0x18))
  {
    Serial.println("Couldnt start");
    while (1)
      ;
  }
  Serial.println("LIS3DH found!");
  lis.setRange(LIS3DH_RANGE_4_G);

  // Initialize NeoPixel Strip
  strip.begin();
  Serial.println("Pixels init'd");

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while (io.status() < AIO_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
}

void updateTime()
{
  // grab the current time from millis()
  currentTime = millis() / 1000;
  seconds = currentTime - prevTime;
  // increase mins.
  if (seconds == 60)
  {
    prevTime = currentTime;
    minutes++;
  }
}

void updatePixels(uint8_t red, uint8_t green, uint8_t blue)
{
  for (int p = 0; p < NUM_PIXELS; p++)
  {
    strip.setPixelColor(p, red, green, blue);
  }
  strip.show();
}

void loop()
{
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  // Update the timer
  updateTime();

  // Get a normalized sensor reading
  sensors_event_t event;
  lis.getEvent(&event);

  // Detect cube face orientation
  if (event.acceleration.x > 9 && event.acceleration.x < 10)
  {
    //Serial.println("Cube TILTED: Left");
    cubeState = 1;
  }
  else if (event.acceleration.x < -9)
  {
    //Serial.println("Cube TILTED: Right");
    cubeState = 2;
  }
  else if (event.acceleration.y < 0 && event.acceleration.y > -1)
  {
    cubeState = 3;
  }
  else
  { // orientation not specified
    //Serial.println("Cube Idle...");
  }

  // return if the orientation hasn't changed
  if (cubeState == prvCubeState)
    return;

  // Send to Adafruit IO based off of the orientation of the cube
  switch (cubeState)
  {
  case 1:
    Serial.println("Switching to Task 1");
    // update the neopixel strip
    updatePixels(50, 0, 0);
    // play a sound
    tone(PIEZO_PIN, 650, 300);
    Serial.print("Sending to Adafruit IO -> ");
    Serial.println(taskTwo);
    cubetask->save(taskTwo, minutes);
    // reset the timer
    minutes = 0;
    break;
  case 2:
    Serial.println("Switching to Task 2");
    // update the neopixel strip
    updatePixels(0, 50, 0);
    // play a sound
    tone(PIEZO_PIN, 850, 300);
    Serial.print("Sending to Adafruit IO -> ");
    Serial.println(taskOne);
    cubetask->save(taskOne, minutes);
    // reset the timer
    minutes = 0;
    break;
  case 3:
    updatePixels(0, 0, 50);
    tone(PIEZO_PIN, 950, 300);
    break;
  }

  // save cube state
  prvCubeState = cubeState;

  // Delay the send to Adafruit IO
  delay(sendDelay * 1000);
}
