TODO:
- Different behavior avr vs pc-based tests to be analyzed
- #include-file structure needs to be cleaned up
- setCurrentPosition() should adjust the maintained last position bits in queue. can be called a bug

pre-0.28.0:
- esp32: fix for compile issue on arduino 1.8.19 as reported by [Issue #127](https://github.com/gin66/FastAccelStepper/issues/127)
- remove deprecated isMotorRunning()
- external callback for pins are now defined on the engine level. This allows generalization for enable and direction pins for esp32. This will break existing code, which have used external enable pin.

0.27.5:
- esp32: fix for compile issue on arduino 1.8.19 as reported by [Issue #117](https://github.com/gin66/FastAccelStepper/issues/117) 

0.27.4:
- esp32s2: fix not moving stepper after forcestop as reported by [Issue #101](https://github.com/gin66/FastAccelStepper/issues/101)

0.27.3:
- esp32/esp32s2: enable all 8/4 rmt backed steppers

0.27.2:
- esp32/esp32s2: fix compilation error of 0.27.1

0.27.1:
- esp32/esp32s2: disable test probes

0.27.0:
- esp32: StepperDemo make p<n> command work again
- esp32: check step pin is a valid gpio output pin
- esp32: Support rmt device with two steppers. so in total 8 steppers for now
- esp32s2: experimental support
- new return code for `addQueueEntry()`: `AQE_DEVICE_NOT_READY`
- `move()` and `moveTo()` expect an optional parameter to opt for blocking operation
- add more `ìnline` in FastAccelStepper.h for short routines to be inlined
- StepperDemo: fix output of usage config

0.26.1:
- after forceStop, the first command was lost

0.26.0:
- avr: Max stepper speed depending on number of assigned steppers:
		1 stepper  => ~76 kHz
		2 steppers => ~37 kHz
		3 steppers => ~25 kHz
- API: add getMaxSpeedIn...() functions
- Remove `MIN_DELTA_TICKS` constant

0.25.7:
- atmega32u4: Disable direct port access function in StepperDemo to free up program memory

0.25.6:
- Internal refactoring: separate RampGenerator and Constant Acceleration Ramp Calculation
- StepperDemo extended:
	- h<speed>: Set the speed in Steps/1000s
- Fix an issue, which has appeared on simavr with overshooting ramp end (test seq 06).
  Actually only the overshoot is avoided, the different behavior avr vs pc-based tests to be analyzed

0.25.5:
- esp32: use busy wait in forwardStep()/backwardStep() instead of yield() as proposed by [Issue #115](https://github.com/gin55/FastAccelStepper/issues/115)
- remove blockingWait for forceStop() in move/moveTo/... as this is actually not required. 

0.25.4:
- Add new function forceStop() as solution for [Issue #116](https://github.com/gin66/FastAccelStepper/issues/116)
- esp32: use yield() instead of vTaskDelay(1) as proposed by [Issue #115](https://github.com/gin55/FastAccelStepper/issues/115)

0.25.3:
- esp32: fix [Issue #113](https://github.com/gin66/FastAccelStepper/issues/113):
    Stepper can fail to start as regression introduced in 0.25.0 and present up to 0.25.2
- sam: fix [Issue #111](https://github.com/gin66/FastAccelStepper/issues/111):
    Stepper can fail to start as regression introduced in 0.25.0 and present up to 0.25.2

0.25.2:
- Fix sketch name of RawOneTurn example
- Fix overshooting due to clipping error
- Minor reverse sqrt-table rework

0.25.1:
- expose internal functions for retrieving queue status as requested by [Issue #112](https://github.com/gin66/FastAccelStepper/issues/112):
	ticksInQueue()
	hasTicksInQueue()
	queueEntries()
- SAM: StepperDemo output function was not defined for this architecture
- Fix for compilation error [Issue #113](https://github.com/gin66/FastAccelStepper/issues/113)
    eventually name clash for min()/max()-definition. Renamed own version to fas_min/fas_max.

0.25.0:
- esp32 changes in response to [Issue #106](https://github.com/gin66/FastAccelStepper/issues/106):
	- Select CPU core to run the StepperTask on by using init(cpu)
	- Trigger the watchdog in the StepperTask
	- Ensure that vDelayTask is not called with zero value,
	  which could be the case for portTICK_PERIOD_MS > 5
- SAM Due: fix issue that call to isRunning() after forceStopAndNewPosition() has returned true
  [Issue #111](https://github.com/gin66/FastAccelStepper/issues/111).
- Rework/simplify internal implementation of queue start for the different architectures.
- ensure forceStopAndNewPosition() is really safe to be called from interrupt
- Refactor internal code for less microcontroller dependency
- isMotorRunning() declared as deprecated and add isQueueRunning() as replacement

0.24.2:
- arduino library manager has not taken up the last minute change
  => bump version witout change

0.24.1:
- Add support for atmega32u4, leonardo
- Reorganized StepperDemo to fit in smaller device like atmega32u4
- support for espidf4.4 for arduino
- experimental (broken ?) support for espidf4.4

0.24.0:
- Implement configurable delay after direction pin change and first step in new direction
  [Issue #83](https://github.com/gin66/FastAccelStepper/issues/83).
  The value is set by the third parameter of setDirectionPin().
- Add table of timing values in FastAccelStepper.h for the architectures
- StepperDemo extended:
	- add config mode (press c) for direction pin configuration
- Support changed mpcwpm-definitions in esp-idf v4.4
- Fix warning identified in this [Issue #98](https://github.com/gin66/FastAccelStepper/issues/98) for `fas_abs()` usage


0.23.5:
- Add missing initialization of dir pin to be `PIN_UNDEFINED`.
  With two or more steppers without dir pin, the steppers have run only sequential and not in parallel.

0.23.4:
- Add support for SAM Due (https://github.com/gin66/FastAccelStepper/pull/82)

0.23.3:
- Add CMakeLists.txt for ESP-IDF (https://github.com/gin66/FastAccelStepper/pull/81)
- avr: use reentrant version for disable/enable interrupts [Issue #75](https://github.com/gin66/FastAccelStepper/issues/75)

0.23.2:
- StepperDemo extended:
	- add test sequence 11 for issue #68
- Fix issue #68: getCurrentPosition() could be off by one command's step amount

0.23.1:
- get actual speed from queue if current and next command has at least one step
- improve accuracy for setSpeedInHz() and setSpeedInMilliHz() and use rounding in addition (issue #56)
- Fix issue #67: If the speed changes are below the minimum speed after one step, then the speed will not change

0.23.0:
- getRampState(): Add two flags for current direction
- add function: getCurrentAcceleration()
- `setAcceleration(uint32_t)` changed to `setAcceleration(int32_t)`. Only positive values allowed.
  This way getCurrentAcceleration() can return negative values without range problems
- add function: getCurrentSpeedInUs(), getCurrentSpeedInMilliHz(), getSpeedInMilliHz()
- restructure tests
- StepperDemo extended:
	- esp32 only: Add command reset, which causes a watchdog reset
	- auto switch between speed in milliSteps/s and us/step depending on command H or V

0.22.2:
- esp32: getCurrentPosition() does take current command's pulse count into consideration
- fix issue #57: counter clear needs a strobe on a bit and not only being set


0.22.1:
- disableOutputs() will return false, if called on a running motor with
  autoEnable set to true
- esp32: replace `pcnt_counter_clear()` by preprocessor-directive in relation to issue #55
- esp32: put const table into RAM due to issue #55

0.22.0:
- Internal code clean up/polishing
- replace getPeriodAfterCommandsCompleted() by
	getPeriodInUsAfterCommandsCompleted() and getPeriodInTicksAfterCommandsCompleted()
- StepperDemo outputs for a running motor the period in us and in ticks

0.21.4:
- PoorManFloat changed from 8bit to 9bit mantissa (implicit msb=1)
   => speed steps with acceleration=1 hardly noticeable
- PoorManFloat new functions for reciprocal calculations

0.21.3:
-  Unidirectional mode: moveByAcceleration() with negative values has used previous acceleration till stop

0.21.2:
- Two new API functions: setSpeedInTicks() and getSpeedInTicks()
- setSpeedInHz() and setSpeedInMilliHz() uses higher resolution than us
- esp32: the spare pulse counter attached to a stepper can now be configured and cleared.
- StepperDemo extended:
	- esp32 only: Add command p<n>,<l>,<h> to configure limits for attached pulse counter
	- esp32 only: Add command pc to clear an attachedPulseCounter
	- H<speed>: Set the speed in Steps/s
- Revert change in 0.21.0 for better resolution of calculate ramp speed => new more tests
- Fix bugs in clipping code, which has caused speed jumps
- Ensure no illegal command in coasting after acceleration change

0.21.1:
- RampGenerator uses now the delayed start feature of the queue
- Fix invalid command generated due to upm calculation tolerance at higher speed

0.21.0:
- StepperDemo extended with test sequence 10 for issue #44
- avr: Avoid ClearInterruptFlag() in ISR, which is needed by simavr due to implementation bug
- eliminate need for FOC workaround by updated simavr
- Better resolution of calculate ramp speed to reduce steps of constant speed while accelerating or decelerating.
  Cost is slower execution of ramp generation.
- Fix a minor bug, that while deleration to a slower set speed, the deceleration could overshoot,
  which requires short acceleration
- StepperDemo extended:
	- command u to put selected stepper into unidirectional mode (need reset to restore)
- Fix for issue #47: Restart in unidirectional mode and position count down
- addQueueEntry returns `AQE_ERROR_NO_DIR_PIN_TO_TOGGLE`, if command defines count down and direction pin is undefined

0.20.2:
- Fix for issue #45: if enablePinHighActive has been set to `PIN_UNDEFINED`,
  the externalEnableCall was cleared even if enablePinLowActive still in use.
- Bugfix for issue #46: Avoid creation of invalid command in ramp generator while decelerating

0.20.1:
- few minor speed ups
- StepperDemo extended:
	- avr only: command 'r' toggles erroneous digitalRead() to stepperpin
	- command 'e' toggles interrupt block of ~100us

0.20.0:
- rename setSpeed to setSpeedInUs
- add setSpeedInHz() and setSpeedInMilliHz()
- RampGenerator code reworked and simplified

0.19.0:
- avoid overflow in setSpeed for super slow speed
- setSpeed and setAcceleration return status code
- moveByAcceleration(): check if direction pin is defined for reverse
- add getter functions: getSpeedInUS(), getAcceleration()
- StepperDemo extended:
	- move/moveTo/moveByAcceleration => verbose error code
	- setSpeed/setAcceleration => report invalid data error
	- motor info with speed/acceleration
	- less verbose: if any motor is running, only info for running motor
- fix issue #43: Issue on moveTo() to exactly same target after a completion of stopMove()


0.18.13:
- Add fix for issue #41: High acceleration and high speed

0.18.12:
- Issue #40 fixed:
	- moveTo() has started ramp generator, even if on target position
	- avoid recalculation of ramp on setAcceleration, if unchanged value

0.18.11:
- Remove obsolete `queue_end` variables ticks and `ticks_from_last_step`
- Fix for issue #33: pulse counter needed to be cleared at motor start

0.18.10:
- Reapply modified fix, which was reverted in 0.18.9

0.18.9:
- Fixed esp32 high speed step loss: partial reverted this fix for now

0.18.8:
- New API-function: isStopping()
- Fixed esp32 high speed step loss

0.18.7
- Fix compile error on esp32

0.18.6
- Running forward/backward could start with previous speed in
  opposite direction due to two missing initialization, which are
  present for move/moveTo

0.18.5
- replace 16bit division with `upm_float` division
- ramp generator packs per command steps for 2 ms or more. 
  Before this was 1ms. This change makes huge difference on Atmega2560

0.18.4:
- avr: Step ISR code optimization e.g. get rid of digitalRead/Write
- esp32: get rid of digitalRead/Write, too
- replave 32bit division with 16bit => much better timing on avr

0.18.3:
- esp32: extend API to attach free pulse counter for debugging
- StepperDemo extended for esp32:
	- p<n>: Attach pulse counter n to the selected stepper

0.18.2:
- extend addQueueEntry() with a start flag. This allows to first fill the queue
  and then start the queue. If several motor are started one after the other
  (with interrupts disabled), a nearly synchronous start is possible.
  Which is required for coordinated axis movement
- make use of this in enqueueing commands from ramp generator

0.18.1:
- `moveByAcceleration()` returns `int8_t` result code
- `setDelayToEnable()` returns an `int8_t` instead of int

0.18.0:
- StepperDemo extended:
	- w<ms>: Option to wait in input processing for some ms
	- test sequence 07 added, which fails on esp32
	- avr: free RAM by usage of `output_msg()`
    - tests can return failure, which is displayed at test end
- Rework RampGenerator: pauses are now after the step and not before. This removes
  some irregularity between steps.
- Rework esp32 interrupt code.
- Fix issue #29 and a slow start of a ramp (identified by #29)
- RampGenerator: Support coasting at lower speed for a couple of steps
  to limit interrupt rate for esp32 (or even rejected commands)
- More tests
- esp32: forceStopAndNewPosition() has not emptied the queue
- fixed: In reversing at max speed an abrupt stop could be initiated
- update FastAccelStepper.h: stopMove() does not make new acceleration value valid
- protect non application fields, which are for test purposes

0.17.1:
- esp32: Fixed one/two spurious step pulses after reset mainly (issue #29)
- avr: reworked ISR for more reliable operation (correct steps). (issue #31)
  Remaining risk (primarily three steppers in parallel in 2560) can cause
  an CPU overload and stepper stopping/not running smooth
- Fix issue #32
- Remove interrupts/noInterrupts protection for shared auto enable function.
  => Thus there is a small time window during auto disable, that a new command issued
     may loose steps. This will be addressed in a subsequent release

0.17.0:
- avr: stepper definition support via AVRStepperPins.h
- 2560: all timers and channels are checked with one test
- addQueueEntry(): remove test for steps >= 128
- new function moveByAcceleration() to control stepper speed by positive and
  negative acceleration values.
- stopMove() will not run to stop, if any move command is called afterwards.
  The error code `MOVE_ERR_STOP_ONGOING` has been removed.
- StepperDemo extended:
	- a: Allows to control the stepper speed by positive/negative acceleration values

0.16.8:
- Fix for issue #30: for dirPin undefined, digitalWrite/pinMode was called

0.16.7:
- Avoid unnecessary direction pin toggle on direction change after motor stop
- Fix `MAX_STEPPER` for atmega2560
- Extend simavr tests for atmega2560 and one test for each timer for both varians

0.16.6:
- bugfix in setEnablePin(): if called with `PIN_UNDEFINED`, that value was used with pinMode and digitalWrite()
- Use simavr to perform regression test for avr on HW-simulation
- Reworked the avr ISR code to avoid steps being lost, if the command queue is running out of commands and a new command comes in with steps to be generated
- Extended the planning ahead time to 20ms.... possibly the load for avr is bit high !? 

0.16.5:
- esp32: forwardStep()/backwardStep() works again
- Fix for issue #25: RampGenerator was irritated, if interrupt has processed command too fast.
- Rework definition of isRunning() for esp32 to make it more like avr

0.16.4:
- Example code for platformio (built with ci/build-platformio.sh) references the library code
  via platformio.ini option and not via symbolic links.
- StepperDemo test mode extended:
	- x: Allows to exit test mode
    - I: Allows to toggle motor info while test sequence is running
    - New test sequence 06 due to issue #24
- Fix for issue #24: unplanned stop due to speed changes during ramp down

0.16.3:
- Add another test case for speed reduction, while running (see issue #23)
  This has identified a bug in the ramp generator impacting esp32/avr. This bug is now fixed.

0.16.2:
- Add comments to example RawAccessWithPause (renamed from RawAccessWithDelay)
- addQueueEntry() expects now a const pointer instead of just a pointer
- Rework RawAccess example and test on hw

0.16.1:
- Usage example in readme has not worked due to a bug in the auto enable/disable variable setting.
  => Added the UsageExample under examples

0.16.0:
- disableOutputs() returns boolean success value
- The external enable output routine will be called regularly in case of disable, too
- esp32: fixed loss of pauses in command queue due to pcnt/mcpwm timer interaction.
   This caused V4100 being faster than V4000, even so it should be slower
- ramp generator: fix for high accelerations to have stop command effective

0.15.2:
- Assume for now, that atmega328 and ATmega2560 uses always same pins for OC1A and OC1B

0.15.1:
- Remove the check for specific avr board. This assumes, that all avr boards
  uses Pin 9 and Pin 10 for OC1A and OC1B respectively

0.15.0:
- For the commands in the queue, the minimum time in ticks to execute
  a command is limited to 10 * `MIN_DELTA_TICKS`. For esp32 this relates to
  the time between interrupts of one channel
- `AQE_ERROR_TICKS_TOO_HIGH` removed, because it is already limited by the data type `uint16_t`
- esp32: Change from mcpwm up count mode to up-down count mode
- avr: adjust implementation to esp32: pulse at start of ticks period of a command
- merge pull request from ixil see (https://github.com/gin66/FastAccelStepper/pull/19)

0.14.0:
- Direction pins can be shared by several steppers
- enableOutputs() returns bool to indicate, output was enabled
- Possibility to supply external enable output control
- AddQueueEntry() return values changed: >0 => retry again, <0 => error
- ATmega2560: Allow to change the used timer module with preprocesser variable `FAS_TIMER_MODULE`

0.13.4:
- Automated github test identified a compile error introduced in 0.13.3

0.13.3:
- esp32: Cyclic rate increased from 10ms to 4ms.
	With a planning ahead time of 10ms, there was the risk
	of running out of commands as identified in issue #18

0.13.2:
- StepperDemo: Compact output for stopped motor
- Fixed a bug, where stepper 4-6 misconfigured stepper 1-3 pcnt.
	=> stepper 4-6 had erroneous behavior for speed <500us
    => stepper 1-3 could in trouble, if corresponding stepper 4-6 was running

0.13.1:
- try a mechanism to include application defined config file
- StepperDemo: for avr move messages into program code to reduce RAM usage

0.13.0:
- Support ATmega2560 for three steppers linked to timer 4 (currently hardcoded)

0.12.2:
- StepperDemo modification:
    - Enable direct drive feature in StepperDemo for esp32
    - While direct driving, check if the signals can be applied
    - Add test mode (enter with t). Here can select stepper, a test sequence and run it
    - In total four test sequences implemented until now
- Add detachFromPin() and reattachToPin() to the API. Shouldn't be used from an application.

0.12.1:
- implement runForward()/runBackward()
- avr: fix interrupt for direction change
- StepperDemo modification:
    - r: Call ESP.restart() to check for issue #6

0.12.0:
- reduce data type for command queue entries' ticks value from `uint32_t` to `uint16_t`
   => remove `ABS_MAX_AQE_TICKS`
- each command in queue can now emit up to 255 steps
- StepperDemo modification:
    - r: Call ESP.restart() to check for issue #6
    - Disable direct drive for esp32
- AutoEnable-Pin can be shared by steppers
- avr: fix interrupt for direction change

0.11.3:
- `ABSOLUTE_MAX_AQE_TICKS` is now 65535

0.11.2:
- auto enable on delay implemented by filling the queue with pause
  This allows approx. 60/120 ms delay for avr/esp32
- esp32: two motors in parallel could lead to uncontrolled running steppers.
         Reason was the wrong registration of the shared interrupt service routine

0.11.1:
- AVR works again
- Limit auto enable on delay to approx. 16ms due to further bugs

0.11.0:
- BROKEN ON AVR
- Slowest speed is `TICKS_PER_S`/0xffffffff, which is ~268s between steps
- `ABS_MAX_TICKS` renamed to `ABS_MAX_AQE_TICKS`. Only applicable to raw commands
- Done: Extend command queue entry to perform delay only without step (steps=0) to reduce the 1.0 steps/s

0.10.0:
- setSpeed() silently imposes lower limit for period
- esp32: step pulse length is for high speed with 50% duty cycle and for low speed fixed at 2ms
- addQueueEntry() receives a `stepper_command_s` struct
- esp32: Task priority of ramp generator task has been set to max Priority.
- StepperDemo extended:
	- Q: Quiet the usage info, which takes time to be transmitted.
      Try this NEMA-17 without load:
			M1 A1000000 V20 P1000 W P0 W P500 W P-500 W P0

0.9.5:
- Fix sudden CPU reset on high interrupt load for avr variant. Issue #12

0.9.4:
- Fix possible race condition in `check_for_auto_disable()`
- StepperDemo extended:
	- blocking wait for stepper stop by press W (dangerous: can deadlock)

0.9.3:
- Fix auto on delay: Delay way always replied, even if the output is still enabled

0.9.2:
- Implement new function applySpeedAcceleration()
- StepperDemo extended:
	- trigger applySpeedAcceleration by press U
- fix possible bug in move/moveTo while keepRunning is set

0.9.1:
- reduce interrupt load on esp32

0.9.0:
- implement forceStopAndNewPosition()
- StepperDemo extended:
	- trigger forceStopAndNewPosition by press X
    - set position with press @
    - keep motor running with press K
- addQueueEntry() returns now an `int8_t` instead of an int
- move and moveTo goes to the closest position (+/-2147483647).
  This means continues move(1000) will let the stepper turn in same direction,
  while the position wraps around: 0,1,...,2147483647,-2147483648,-2147483647,...,-1,0,1,...
- add keepRunning() to let the motor continuously run in same direction.
- rename isrSpeedControlEnabled() to isRampGeneratorActive()

0.8.3:
- AVR: timer compare interrupts are only enabled, if stepper is running.
- AVR: on arrival of a command, the queue is started with few µs delay
- Implement backwardStep() and forwardStep()
- Bug solved: Speed changes at very low speed with high acceleration values are not always performed
  Actually speed with period times > 268436µs has not worked at all before.

0.8.2:
- Solved issue: Queue is filled too much, which cause slow response to speed/acceleration changes
  => Queue is filled to max ~10ms into the future.

0.8.1:
- Fix issue #8: Long step times are less accurate than short ones
  => All time delta between steps are cycle accurate
- Add getPeriodAfterCommandsCompleted() to API
- Fix bug due to AutoEnable at ramp start (can find at low speeds)
- StepperDemo outputs: `F_CPU`/`TICKS_PER_S` and stepper period at queue end

0.8.0:
- Change direction with running motor is possible !!!
- stopMove() can be called from interrupt routine
- Refactor ramp generation code into RampGenerator.h/cpp
- StepperDemo: ramp state is written as plain text
- Mention platformio in README

0.7.1:
- StepperDemo extended with commands to 
		return status code from move/moveTo
		toggle motor info (I) to suppress info while steppers are running
		output usage (?)
		output motor info with usage
		test direct drive of stepper by port manipulation bypassing the library (T)
- move/moveTo return error codes

0.7.0: Changes towards 0.6.15
- Fix possible floating point exception (divide by zero), which could happen rarely in `isr_single_fill_queue`
- Remove deprecated functions:
	addQueueStepperStop()
    isStopped()
- internal: remove obsolete `_stepper_num variable`
