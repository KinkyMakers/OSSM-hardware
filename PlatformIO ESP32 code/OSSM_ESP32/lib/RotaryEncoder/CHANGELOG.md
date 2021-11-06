# Changelog

All notable changes to this project will be documented in this file starting 2021.

## [1.5.2] - 2021-07-04

minor fixes found in ci integration.


## [1.5.1] - 2021-04-23

FIX: attachInterrupt() with digitalPinToInterrupt() required for Arduino UNO etc.


## [1.5.0] - 2021-02-06

* CHANGELOG created.

* Many thanks to the improvements included from **@deltaphi** (#12), **@rprediger** (#22) and **@giupo** (#24) 

* The function **getMillisBetweenRotations()** was added. See example AcceleratedRotator.

* The function **getRPM()** was added.


### Examples

* The examples also compile with ESP8266 and Every boards. (the library worked already).

* new **LatchMode** in initialization supporting rotary encoders with 2 state changes from one latch to the next (LatchMode).

* The **InterruptRotator** example now uses attachInterrupt() instead of ATMega registers and ISR. (The old code is still in comments).

