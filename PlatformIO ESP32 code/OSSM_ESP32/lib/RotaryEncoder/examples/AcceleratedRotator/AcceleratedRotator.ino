// -----
// AcceleratedRotator.ino - Example for the RotaryEncoder library.
// This class is implemented for use with the Arduino environment.
// Copyright (c) by Matthias Hertel, http://www.mathertel.de
// This work is licensed under a BSD style license. See http://www.mathertel.de/License.aspx
// More information on: http://www.mathertel.de/Arduino
// -----
// 18.01.2014 created by Matthias Hertel
// 13.11.2019 converted to AcceleratedRotator by Damian Philipp
// 06.02.2021 conditions and settings added for ESP8266
// -----

// This example checks the state of the rotary encoder in the loop() function.
// It then computes an acceleration value and prints the current position when changed.
// There is detailed output given to the Serial.
// You may play around with the constants to fit to your needs.

// Hardware setup:
// Attach a rotary encoder with output pins to
// * 2 and 3 on Arduino UNO.
// * A2 and A3 can be used when directly using the ISR interrupts, see comments below.
// * D5 and D6 on ESP8266 board (e.g. NodeMCU).
// Swap the pins when direction is detected wrong.
// The common contact should be attached to ground.

#include <Arduino.h>
#include <RotaryEncoder.h>

#if defined(ARDUINO_AVR_UNO) || defined(ARDUINO_AVR_NANO_EVERY)
// Example for Arduino UNO with input signals on pin 2 and 3
#define PIN_IN1 2
#define PIN_IN2 3

#elif defined(ESP8266)
// Example for ESP8266 NodeMCU with input signals on pin D5 and D6
#define PIN_IN1 D5
#define PIN_IN2 D6

#endif

// Setup a RotaryEncoder with 4 steps per latch for the 2 signal input pins:
// RotaryEncoder encoder(PIN_IN1, PIN_IN2, RotaryEncoder::LatchMode::FOUR3);

// Setup a RotaryEncoder with 2 steps per latch for the 2 signal input pins:
RotaryEncoder encoder(PIN_IN1, PIN_IN2, RotaryEncoder::LatchMode::TWO03);

// Define some constants.

// the maximum acceleration is 10 times.
constexpr float m = 10;

// at 200ms or slower, there should be no acceleration. (factor 1)
constexpr float longCutoff = 50;

// at 5 ms, we want to have maximum acceleration (factor m)
constexpr float shortCutoff = 5;

// To derive the calc. constants, compute as follows:
// On an x(ms) - y(factor) plane resolve a linear formular factor(ms) = a * ms + b;
// where  f(4)=10 and f(200)=1

constexpr float a = (m - 1) / (shortCutoff - longCutoff);
constexpr float b = 1 - longCutoff * a;

// a global variables to hold the last position
static int lastPos = 0;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;

  Serial.println("AcceleratedRotator example for the RotaryEncoder library.");
  Serial.print("a=");
  Serial.println(a);
  Serial.print("b=");
  Serial.println(b);
} // setup()


// Read the current position of the encoder and print out when changed.
void loop()
{
  encoder.tick();

  int newPos = encoder.getPosition();
  if (lastPos != newPos) {

    // accelerate when there was a previous rotation in the same direction.

    unsigned long ms = encoder.getMillisBetweenRotations();

    if (ms < longCutoff) {
      // do some acceleration using factors a and b

      // limit to maximum acceleration
      if (ms < shortCutoff) {
        ms = shortCutoff;
      }

      float ticksActual_float = a * ms + b;
      Serial.print("  f= ");
      Serial.println(ticksActual_float);

      long deltaTicks = (long)ticksActual_float * (newPos - lastPos);
      Serial.print("  d= ");
      Serial.println(deltaTicks);

      newPos = newPos + deltaTicks;
      encoder.setPosition(newPos);
    }

    Serial.print(newPos);
    Serial.print("  ms: ");
    Serial.println(ms);
    lastPos = newPos;
  } // if
} // loop ()

// The End
