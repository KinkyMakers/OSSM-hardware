# FastAccelStepper 
 
![GitHub tag](https://img.shields.io/github/v/tag/gin66/FastAccelStepper.svg?sort=semver&no_cache_0.24.1)
[![PlatformIO Registry](https://badges.registry.platformio.org/packages/gin66/library/FastAccelStepper.svg)](https://registry.platformio.org/libraries/gin66/FastAccelStepper)

![Run tests](https://github.com/gin66/FastAccelStepper/workflows/Run%20tests/badge.svg?no_cache_0.24.1)
![Simvar tests](https://github.com/gin66/FastAccelStepper/workflows/Run%20tests%20with%20simavr/badge.svg?no_cache_0.24.1)

[![Build examples for esp32](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32.yml)
[![Build examples for esp32s2](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32s2.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32s2.yml)
[![Build examples for esp32arduino @ 3.4.0](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32arduinoV340.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32arduinoV340.yml)
[![Build examples for esp32arduino @ latest](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32arduinolatest.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_esp32arduinolatest.yml)
[![Build examples for Atmega2560](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_atmega2560.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_atmega2560.yml)
[![Build examples for Atmel SAM](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_atmelsam.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_atmelsam.yml)
[![Build examples for Atmega328](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_nanoatmega328.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_nanoatmega328.yml)
[![Build examples for Atmega32U4](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_atmega32u4.yml/badge.svg)](https://github.com/gin66/FastAccelStepper/actions/workflows/build_examples_nanoatmega32u4.yml)

This is a high speed alternative for the [AccelStepper library](http://www.airspayce.com/mikem/arduino/AccelStepper/).
Supported are avr (ATmega 328, ATmega2560), esp32, esp32s2 and atmelsam due.

The stepper motors should be connected via a driver IC (like A4988) with a 1, 2 or 3-wire connection:
* Step Signal
	- avr atmega328p: only Pin 9 and 10.
	- avr atmega32u4: only Pin 9, 10 and 11.
	- avr atmega2560: only Pin 6, 7 and 8.
      On platformio, this can be changed to other triples: 11/12/13 Timer 1, 5/2/3 Timer 3 or 46/45/44 Timer 5 with FAS_TIMER_MODULE setting.
	- esp32: This can be any output capable port pin.
    - atmel sam due: This can be one of each group of pins: 34/67/74/35, 17/36/72/37/42, 40/64/69/41, 9, 8/44, 7/45, 6
	- Step should be done on transition Low to High. High time will be only a few us.
      On esp32 the high time is for slow speed fixed to ~2ms and high speed to 50% duty cycle
* Direction Signal (optional)
	- This can be any output capable port pin.
    - Position counting up on direction pin high or low, as per optional parameter to setDirectionPin(). Default is high.
    - With external callback on esp32 derivates, even shift register outputs can be used
* Enable Signal (optional)
	- This can be any output capable port pin.
    - Stepper will be enabled on pin high or low, as per optional parameter to setEnablePin(). Default is low.
    - With external callback, even shift register outputs can be used

FastAccelStepper offers the following features:
* 1-pin operation for e.g. peristaltic pump => only positive move
* 2-pin operation for e.g. axis control
* 3-pin operation to reduce power dissipation of driver/stepper
* Lower limit of 260s per step @ 16MHz aka one step every four minute (esp32/avr), 198s for sam due
* fully interrupt/task driven - no periodic function to be called from application loop
* supports acceleration and deceleration with per stepper max speed/acceleration
* Allows the motor to continuously run in the current direction until stopMove() is called.
* speed/acceleration can be varied while stepper is running (call to functions move or moveTo is needed in order to apply the new values)
* Constant acceleration control: In this mode the motor can be controled by acceleration values and with acceleration=0 will keep current speed
* Auto enable mode: stepper motor is enabled before movement and disabled afterwards with configurable delays
* Enable pins can be shared between motors
* Direction pins can be shared between motors
* Configurable delay between direction change and following step
* External callback function can be used to drive the enable pins (e.g. connected to shift register) and, only esp32 derivates: the direction pins
* No float calculation (use own implementation of poor man float: 8 bit mantissa+8 bit exponent)
* Provide API to each steppers' command queue. Those commands are tied to timer ticks aka the CPU frequency!
* Command queue can be filled with commands and then started. This allows near synchronous start of several steppers for multi axis applications.

General behaviour:
* The desired end position to move to is set by calls to moveTo() and move()
* The desired end position is in case of moveTo() given as absolute position
* For move() the delta is added to the latest desired end position
* The stepper tries to reach the given desired end position as fast as possible with adherence to acceleration/deceleration
* If the stepper is e.g. running towards position 1000 and moveTo(0) is called at position 500, then the stepper will 
    1. decelerate, which means it will overshoot position 500 
    2. stop and accelerate towards 0
    3. eventually coast for a while and then decelerate
    4. stop
* The stepper position is a 32bit integer variable, which wraps around for continuous movement.
  Example:
	- Assume counting up turns stepper clockwise, and counting down, anti-clockwise.
    - Current position is -2.000.000.000, move to 2.000.000.000.
    - Apparently the position has to count up, and count should run clockwise.
    - Implementation is done via difference of 32bit signed numbers, which can overflow (being legal).
    - The calculation is then:
			2.000.000.000 - (-2.000.000.000) = 4.000.000.000
	- But 4.000.000.000 interpreted as signed 32bit is -294.967.296 => count down, turn anti-clockwise
	  Means the position will count:
```
		-2.000.000.000
		-2.000.000.001
		-2.000.000.002
			:
		-2.147.483.647
		-2.147.483.648
		 2.147.483.647
		 2.147.483.646
		 2.147.483.645
			:
		 2.000.000.000
```
Comments to pin sharing:
* Enable pin sharing: the common pin will be enabled for as long as one motor is running + delay off.
  Every motor will adhere to its auto enable delay, even if other motors already have enabled the pin.
* Direction pin sharing: The direction pin will be exclusively driven by one motor. If one motor is operating, another motor will wait until the direction pin comes available

### AVR ATMega 328

* allows up to 76000 generated steps per second for single stepper operation, 37000 for dual stepper
* supports up to two stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Uses `F_CPU` Macro for the relation tick value to time, so it should now not be limited to 16 MHz CPU frequency (untested)
* Steppers' command queue depth: 16

### AVR ATMega 32u4

* allows up to 76000 generated steps per second for single stepper operation, 37000 for dual stepper and 25000 for three steppers
* supports up to three stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Uses `F_CPU` Macro for the relation tick value to time, so it should now not be limited to 16 MHz CPU frequency (untested)
* Steppers' command queue depth: 16

### AVR ATMega 2560

* allows up to 76000 generated steps per second for single stepper operation, 37000 for dual stepper and 25000 for three steppers
* supports up to three stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Uses `F_CPU` Macro for the relation tick value to time, so it should now not be limited to 16 MHz CPU frequency (untested)
* Steppers' command queue depth: 16
* This device has four 16 bit timers, so extension up to 12 steppers should be possible (not implemented)

### ESP32

* allows up to 200000 generated steps per second
* supports up to 14 stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Steppers' command queue depth: 32

### ESP32S2

* reported to work
* allows up to 200000 generated steps per second ?
* supports up to four stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Steppers' command queue depth: 32

### Atmel SAM Due

* allows up to 50000 generated steps per second
* supports up to six stepper motors using Step/Direction/Enable Control (Direction and Enable is optional)
* Steppers' command queue depth: 32

Tested with max two stepper motors with 50 kHz step rate by clazarowitz

## Usage

The library is in use with A4988, but other driver ICs should work, too.

For the API definition please consult the header file [FastAccelStepper.h](https://github.com/gin66/FastAccelStepper/blob/master/src/FastAccelStepper.h). Or a generated [markdown file](https://github.com/gin66/FastAccelStepper/blob/master/extras/doc/FastAccelStepper_API.md)

Please check the examples for application and how to use the low level interface.
Some info is [Issue #86](https://github.com/gin66/FastAccelStepper/issues/86).

The module defines the global variable `fas_queue`. Do not use or redefine this variable.

Using the high level interface with ramp up/down as in [UsageExample.ino](https://github.com/gin66/FastAccelStepper/blob/master/examples/UsageExample/UsageExample.ino).

```
#include "FastAccelStepper.h"
#include "AVRStepperPins.h" // Only required for AVR controllers

#define dirPinStepper    5
#define enablePinStepper 6
#define stepPinStepper   9

// If using an AVR device use the definitons provided in AVRStepperPins
//    stepPinStepper1A
//
// or even shorter (for 2560 the correct pin on the chosen timer is selected):
//    stepPinStepperA

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

void setup() {
   engine.init();
   stepper = engine.stepperConnectToPin(stepPinStepper);
   if (stepper) {
      stepper->setDirectionPin(dirPinStepper);
      stepper->setEnablePin(enablePinStepper);
      stepper->setAutoEnable(true);

      stepper->setSpeedInHz(500);       // 500 steps/s
      stepper->setAcceleration(100);    // 100 steps/s²
      stepper->move(1000);
   }
}

void loop() {
}
```

Few comments to auto enable/disable:

* If the motor is operated with micro stepping, then the disable/enable will cause the stepper to jump to/from the closest full step position.
* Some drivers need time to e.g. stabilize voltages until stepping should start. For this the start on delay has been added. See [issue #5](https://github.com/gin66/FastAccelStepper/issues/5).
* The turn off delay is realized in the cyclic task for esp32 or cyclic interrupt for avr. The esp32 task uses 4ms delay, while the avr repeats every ~4 ms at 16 MHz and atmel sam due every 2ms at 21MHz. Thus the turn off delay is a multiple (n>=2) of those period times and actual turning off takes place approx [(n-1)..n] * 4 ms resp. 2ms after the last step.
* The turn on delay is minimal `MIN_CMD_TICKS`.
* More than one stepper can be connected to one auto enable pin. Behaviour is like this:
	1. If stepper #1 needs enable, then it will enable it with its defined on delay time.
    2. If stepper #2, which is connected to same enable pin, starts after stepper one, then it still will wait its defined on delay time and set the enable pin, again (no-op).
       The stepper #2 is not aware, that another stepper (stepper #1) has enabled the outputs already.
    3. If e.g. stepper #1 stops, then stepper #1's delay off counter is started.
    4. When stepper #1's counter is finished, then the FastAccelStepperEngine will ask all steppers, if they agree to stepper #1's disable request.
       If stepper #2 is still running, then stepper #2 will not agree and the output will stay enabled.
    5. When stepper #2 stops, then stepper #2's delay off counter is started.
    6. When stepper #2's counter is finished, then the FastAccelStepperEngine will ask all steppers, if they agree to stepper #2's disable request.
       Stepper #1 agrees, because it is not running. So the engine will call Stepper #2's _AND_ Stepper #1's disableOutputs().

  The library does not consider the case, that Low/High Active enable may be mixed.
  This means stepper #1 uses the enable pin as High Active and stepper #2 the same pin as Low Active.
  => This situation will not be identified and will lead to unexpected behaviour

## Behind the curtains

### AVR ATmega328 and Atmega32u4

The timer 1 is used with prescaler 1. With the arduino nano running at 16 MHz, timer overflow interrupts are generated every ~4 ms. This timer overflow interrupt is used for adjusting the speed. 

The timer compare unit toggles the step pin from Low to High precisely. The transition High to Low is done in the timer compare interrupt routine, thus the High state is only few us.

After stepper movement is completed, the timer compare unit is disconnected from the step pin. Thus the application could change the state freely, while the stepper is not controlled by this library.

Measurement of the acceleration/deacceleration aka timer overflow interrupt yields: one calculation round needs around 300us. Thus it can keep up with the chosen 10 ms planning ahead time.

### AVR ATmega2560

Similar to ATmega328, but instead of timer 1, timer 4 is used.

For users of platformio, the used timer can be changed to either 1, 3, 4 or 5. For e.g. timer module 3 add to platformio.ini under `build_flags`:
```
build_flags = -DFAS_TIMER_MODULE=3
```

or better:
```
build_flags = -Werror -Wall -DFAS_TIMER_MODULE=3
```

For arduino users, the same can be done by defining the flag *before* including the `FastAccelStepperEngine.h` header (as per info ixil), but apparently to [issue #50](https://github.com/gin66/FastAccelStepper/issues/50), this approach does not work for everyone:
e.g.
```
sketch.ino
----------
#include <Arduino.h>
#define FAS_TIMER_MODULE 3
#include <FastAccelStepper.h>
/* ... */
```

### ESP32

This stepper driver uses mcpwm modules of the esp32: for the first three stepper motors mcpwm0, and mcpwm1 for the steppers four to six.  In addition, the pulse counter module is used starting from `unit_0` to `unit_5`. This driver uses the `pcnt_isr_service`, so unallocated modules can still be used by the application. The mcpwm modules' outputs are fed into the pulse counter by direct gpio-matrix-modification.

For the other stepper motors, the rmt module comes into use.

A note to `MIN_CMD_TICKS` using mcpwm/pcnt: The current implementation uses one interrupt per command in the command queue. This is much less interrupt rate than for avr. Nevertheless at 200kSteps/s the switch from one command to the next one should be ideally serviced before the next step. This means within 5us. As this cannot be guaranteed, the driver remedies an overrun (at least by design) to deduct the overrun pulses from the next command. The overrun pulses will then be run at the former command's tick rate. For real life stepper application, this should be ok. To be considered for raw access: Do not run many steps at high rate e.g. 200kSteps/s followed by a pause. 

What are the differences between mcpwm/pcnt and rmt ?

|                            | mcpwm/pcnt                              | rmt                                                                           |
|:---------------------------|:----------------------------------------|:------------------------------------------------------------------------------|
|Interrupt rate/stepper      | one interrupt per command               | min: one interrupt per command, max: one interrupt per 31 steps at high speed |
|Required interrupt response | at high speed: time between two steps   | at high speed: time between 31 steps                                          |
|Module usage                | 1 or 2 mcpcms, up to 6 channels of pcnt | rmt                                                                           |
|esp32 notes                 | availabe pcnt modules can be connected  | no pcnt module used, so can be attached to rmt output as realtime position    |

If the interrupt load is not an issue, then rmt is the better choice. With rmt the below (multi-axis application) mentioned loss of synchonicity at high speeds can be avoided. The rmt driver is - besides some rmt modules perks - less complex and way more straightforward.

As of now, allocation of steppers on esp32 are: first all 6 mcpwm/pcnt drivers and then the 8 rmt drivers. In future this may be under application control.

### ESP32S2

This stepper driver uses rmt module.

### ESP32C3/ESP32-MINI-1

Compatibility with ESP32-C3: Not supported currently. The rmt module has more changes compared to esp32/esp32s2

Compatibility with ESP32-MINI-1: At least mcpwm and pulse counter modules are listed in the datasheet. So there are chances, that this lib works.

### Atmel SAM Due

This is supported by clazarowitz

### ALL

The used formula is just s = 1/2 * a * t² = v² / (2 a) with s = steps, a = acceleration, v = speed and t = time. In order to determine the speed for a given step, the calculation is v = sqrt(2 * a * s). The performed square root is an 8 bit table lookup. Sufficient exact for this purpose.

The compare interrupt routines use 16bit tick counters, which translates to approx. 4ms. For longer time between pulses, pauses without step output can be added. With this approach the ramp generation supports up to one step per 268s. 

The low level command queue for each stepper allows direct speed control - when high level ramp generation is not operating. This allows precise control of the stepper, if the code, generating the commands, can cope with the stepper speed (beware of any Serial.print in your hot path).

## Usage for multi-axis applications

For coordinated movement of two or more axis, the current ramp generation will not provide good results. The planning of steps needs to take into consideration max.speed/acceleration of all steppers and eventually the net speed/acceleration of the resulting movement together with its restrictions. Nice example of multi-axis forward planning can be found within the [marlin-project](https://github.com/MarlinFirmware/Marlin/tree/2.0.x/Marlin/src/module). If this kind of multi-dimensional planning is used, then FastAccelStepper is a good solution to execute the raw commands (without ramp generation) with near-synchronous start of involved steppers. With the tick-exact execution of commands, the synchronization will not be lost as long as the command queues are not running out of commands. And for esp32, second requirement is, that the interrupts can be serviced on time (no pulses issued with previous command's pulse period time)

To keep up the synchronization of two steppers please keep in mind:
* The stepper queue will on initial start, if configured, add pauses to the command queue to implement enable delay.
  => Perhaps best to not use enable on delay
* If the stepper is configured for delays for direction change, then one pause is added to the command queue for each direction change together with a step.
  => Execute direction change together with a pause or do not configure direction change delay

## TODO

See [project](https://github.com/gin66/FastAccelStepper/projects/1)

## Arduino

[Arduino library manager log](https://downloads.arduino.cc/libraries/logs/github.com/gin66/FastAccelStepper/)

## PLATFORMIO

[Library on platformio](https://registry.platformio.org/libraries/gin66/FastAccelStepper)

If you prefer platformio and you are running Linux, then platformio version of the examples are created by executing

```
ci/build-platformio.sh
```

This will create a directory pio_dirs, which contains all examples. Can be executed by e.g.

```
cd pio_dirs/StepperDemo
pio run -e avr --target upload --upload-port /dev/ttyUSB0
```

## ESP-IDF

A `CMakeLists.txt` is provided to use FastAccelStepper as an ESP-IDF component. Clone it into the `components/` directory in the root of your project and build as usual. You must have Arduino available as a component. [See this](https://docs.espressif.com/projects/arduino-esp32/en/latest/esp-idf_component.html) for instructions on how to set that up. Tested as ESP-IDF component on PlatformIO Espressif32 Platform v3.3.2.

For any questions/support please contact [gagank1](https://github.com/gagank1), as I do not use esp-idf

## TEST STRATEGY

The library is tested with different kind of tests:
* PC only (sub folder `./tests/pc_based`)

  These tests focussing primarily the ramp generator and part of the API
* simavr based for avr (sub folder `./tests/simavr_based`)

  The simavr is an excellent simulator for avr microcontrollers. This allows to check the avr implementation thoroughly: number of steps generated, virtual stepper position and even timing. Tested code is mainly the StepperDemo, which gets fed in a one line sequence of commands to execute. These tests are focused on avr, but help to check the whole library code, used by esp32, too.
* esp32 tests with another pulse counter attached (e.g. `test_seq_08` in StepperDemo)

  The FastAccelStepper-API supports to attach another free pulse counter to a stepper's step and dir pins. This counter counts in the range of -16383 to 16383 with wrap around to 0. The test condition is, that the library's view of the position should match the independently counted one. These tests are still evolving

* Test for pulse generation using examples/Pulses

  This has been intensively used to debug the esp32 ISR code

* esp32 hw tests

  These tests live under sub folder `./tests/esp32_hw_based`

* manual tests using examples/StepperDemo

  These are unstructured tests with listening to the motor and observing the behavior

## Test sequences from StepperDemo

Short info, what the test sequences, embedded in StepperDemo in the test mode, do:

- 01 - Run the stepper like a clock for one minute
- 02 - Run the stepper towards positive position and back to zero repeatedly
- 03/04 - same like 02, both different speed/acceleration
- 05 - Perform 800 times a single step and then 800 steps back in one command
- 06 - Run 32000 steps with speed changes every 100ms in order to reproduce issue #24

All those tests have no internal test passed/failed criteria. Those are used for automated tests with `./tests/simavr_based` and `./tests/esp32_hw_based`. The test pass criteria are: They should run smoothly without hiccups and return to start position.

- 07 - measures timing of several moveByAcceleration(). Should stop at position zero. (should be started from position 0).
- 08 - is an endless running test to check on esp32, if the generated pulses are successfully counted by a second pulse counter. The moves should be all executed in one second with alternating direction and varying speed/acceleration
- 09 - is an endless running test with starting a ramp with random speed/acceleration/direction, which after 1s is stopped with forceStopAndNewPosition(). It contains no internal test criteria, but looking at the log, the match of generated and sent pulses can be checked. And the needed steps for a forceStopAndNewPosition() can be derived out of this
- 10 - runs the stepper forward and every 200 ms changes speed with increasing positive speed deltas and then decreasing negative speed deltas.
- 11 - runs the stepper to position 1000000 and back to 0. This tests, if getCurrentPosition() is counting monotonously up or down respectively.

## CHANGELOG

See [changelog](https://github.com/gin66/FastAccelStepper/blob/master/CHANGELOG)

## ISSUES

* There is an issue with the esp32 mcpwm: as soon as the mcpwm timer is running, on every cycle an interrupt is serviced - even though no interrupt is enabled. If several steppers are running at high step rate, the interrupt load for this nonsense interrupt could be quite high for the CPU. Need further investigation, but till now haven't found the root cause.
* Compilation using esp-idf 4.4 will yield a deprecation warning for `mcpwm_isr_register()`. This has been raised as [issue](https://github.com/espressif/esp-idf/issues/7890) at espressif
* `framework-arduinoespressif32 @ 3.10006.210326` and later will lead to compile error for esp32, if using compiler options `-Werror -Wall` !!! The problem can be circumvented by applying `-Wno-error=incompatible-pointer-types`


## Error investigation

In case the stepper does not run smoothly, then StepperDemo contains commands to simulate two type of error causes. For avr the commands are `r` and `e`. For esp32 only `e` can be used.
- `r`: The digitalRead() of arduino is a fancy implementation, which checks, if the pin being read is connected to a timer to generate PWM and if yes, turns this off (actually IMHO a broken implementation: only 1 of the needed 2 bits are cleared, and the activation by force compare is missing). As FastAccelStepper controls the step pin, the digitalRead can disturb the step pin (even though I have expected step loss, only difference in noise can be heard). The error simulation in StepperDemo reads the pins in the main loop(), thus the symptom occurs quite reliably.
- `e`: This blocks repeatedly interrupts for ~100us during 64ms out of 256ms. On AVR to see this problem popping up, the stepper rate has to be <~106 us (avr, one stepper running). >~106us it runs quite smoothly. The 106us = 100us block + ~6us ISR runtime. For ESP32 this has no effect.

For avr: cause of long interrupt being blocked can be e.g.:
- long section of codes between `noInterrupts()/interrupts()` in the application (or used libraries)
- long interrupt service routines in the application (or used libraries).
- port interrupts connected to noisy/bouncy switches causing bursts of interrupts

Especially in interrupt service routines, the `digitalRead()/digitalWrite()` must be avoided. Alternative solution is described e.g. here: [blog](https://masteringarduino.blogspot.com/2013/10/fastest-and-smallest-digitalread-and.html), or [digitalWriteFast](https://github.com/NicksonYap/digitalWriteFast), or [fast versions](https://forum.arduino.cc/index.php?topic=46896.0).

This feature of StepperDemo allows to compare non-smooth running stepper in an application with these error types.

## Lessons Learned

* Spent more than half a day debugging the esp32-code, till I have found out, that just the cable to the stepper was broken.
* In one setup, operating A4988 without microsteps has led to erratic behaviour at some specific low speed (erratic means step forward/backward, while DIR is kept low). No issue with 16 microstep. These two youtube videos show similar behavior: [hard disc stepper](https://youtu.be/DsYgw3GFHZo) and [axes movement](https://youtu.be/Nw18B81Ylhk)
* The pulse counters in esp32 have several comparators to trigger interrupts. What the documentation does not mention: All those reference values are only forwarded to the actual comparator on pulse counter reset. Thus the pulse counters cannot be used as lower 16bit of the position, unfortunately.
* The [issue #60](https://github.com/gin66/FastAccelStepper/issues/60) was raised due to wrong position on negative moves with esp32. Apparently the issue was with proper ground and/or power lines to the stepper driver. If similar issue is encountered, please check on this issue

## 3rd party videos in action

Found on youtube:

* [Testing on NEMA-17](https://www.youtube.com/watch?v=yUTXTRjAOak)
* [high speed Closed Loop nema 34 12nm stepper motor, esp32 FastAccelStepper, SBH860H driver](https://www.youtube.com/watch?v=hPxJekex5zM)
* [Neck mechanism](https://www.youtube.com/watch?v=rY7NDBnz7Cw)
* [Stepper motor at 1500RPM with ESP32 and A4988](https://www.youtube.com/watch?v=sQqezEsiuUU)
* [DIY 3 AXIS CAMERA SLIDER | MOTORIZED CAMERA SLIDER](https://youtu.be/7TkybpSQULk)
* [NEMA 17 Servo: Final Accuracy Test & New Speed Record!! - N-Gnoid TV](https://www.youtube.com/watch?v=EHHDuI3xK94)

As mentioned by kthod861 in [Issue #110](https://github.com/gin66/FastAccelStepper/issues/110):
* [22 01 2021 Stepper POC3](https://youtu.be/fm2_VkUG10k)

## Contribution

- Thanks ixil for pull request (https://github.com/gin66/FastAccelStepper/pull/19) for ATmega2560
- Thanks gagank1 for esp-idf integration by adding CMakeLists.txt (https://github.com/gin66/FastAccelStepper/pull/81)
- Thanks clazarowitz for the amazing atmel sam due port (https://github.com/gin66/FastAccelStepper/pull/82)

