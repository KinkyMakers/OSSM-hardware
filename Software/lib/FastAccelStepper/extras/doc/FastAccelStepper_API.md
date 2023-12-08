# FastAccelStepper

FastAccelStepper is a high speed alternative for the
[AccelStepper library](http:www.airspayce.com/mikem/arduino/AccelStepper/).
Supported are avr (ATmega 328, ATmega2560), esp32 and atmelsam due.

Here is a basic example to run a stepper from position 0 to 1000 and back
again to 0.
```

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

#define dirPinStepper    5
#define enablePinStepper 6
#define stepPinStepper   9
void setup() {
   engine.init();
   stepper = engine.stepperConnectToPin(stepPinStepper);
   if (stepper) {
      stepper->setDirectionPin(dirPinStepper);
      stepper->setEnablePin(enablePinStepper);
      stepper->setAutoEnable(true);

      stepper->setSpeedInHz(500);
      stepper->setAcceleration(100);
      stepper->moveTo(1000, true);
      stepper->moveTo(0, true);
   }
}

void loop() {}
```

## FastAccelStepperEngine

This engine - actually a factory - provides you with instances of steppers.
### Initialization

The FastAccelStepperEngine is declared with FastAccelStepperEngine().
This is to occupy the needed memory.
```cpp
FastAccelStepperEngine engine = FastAccelStepperEngine();
```
But it still needs to be initialized.
For this init shall be used:
```cpp
void setup() {
   engine.init();
}
```
In a multitasking and multicore system like ESP32, the steppers are
controlled by a continuously running task. This task can be fixed to one
CPU core with this modified init()-call. ESP32 implementation detail: For
values 0 and 1, xTaskCreatePinnedToCore() is used, or else xTaskCreate()
```cpp
  void init(uint8_t cpu_core);
```
### Creation of FastAccelStepper

Using a call to `stepperConnectToPin()` a FastAccelStepper instance is
created. This call tells the stepper, which step pin to use. As the
hardware may have limitations - e.g. no stepper resources anymore, or the
step pin cannot be used, then NULL is returned. So it is advised to check
the return value of this call.
```cpp
  FastAccelStepper* stepperConnectToPin(uint8_t step_pin);
```
Comments to valid pins:

| Device     | Comment                                                                                           |
|:-----------|:--------------------------------------------------------------------------------------------------|
| ESP32      | Every output capable GPIO can be used                                                             |
| ESP32S2    | Every output capable GPIO can be used                                                             |
| Atmega328p | Only the pins connected to OC1A and OC1B are allowed                                              |
| Atmega2560 | Only the pins connected to OC4A, OC4B and OC4C are allowed.                                       |
| Atmega32u4 | Only the pins connected to OC1A, OC1B and OC1C are allowed                                        |
| Atmel SAM  | This can be one of each group of pins: 34/67/74/35, 17/36/72/37/42, 40/64/69/41, 9, 8/44, 7/45, 6 |
## External Pins

If the direction/enable pins are e.g. connected via external HW (shift registers),
then an external callback function can be supplied.
The supplied value is either LOW or HIGH. The return value shall be
the status of the pin (either LOW or HIGH). If returned value and supplied
value do not match, the stepper does not continue, but calls this function again.

This function is called from cyclic task/interrupt with 4ms rate, which creates
the commands to put into the command queue.
Thus the supplied function should take much less time than 4ms.
Otherwise there is risk, that other running steppers are running out of
commands in the queue. If this takes longer, then the function should be
offloaded and return the new status, after the pin change has
been successfully completed.
```cpp
  void setExternalCallForPin(bool (*func)(uint8_t pin, uint8_t value));
```
### Debug LED

If blinking of a LED is required to indicate, the stepper controller is
still running, then the port. to which the LED is connected, can be told to
the engine. The periodic task will let the associated LED blink with 1 Hz
```cpp
  void setDebugLed(uint8_t ledPin);
```
### Return codes of calls to `move()` and `moveTo()`

All is OK:
```cpp
#define MOVE_OK 0
```
Negative direction requested, but no direction pin defined
```cpp
#define MOVE_ERR_NO_DIRECTION_PIN -1
```
The maximum speed has not been set yet
```cpp
#define MOVE_ERR_SPEED_IS_UNDEFINED -2
```
The acceleration to use has not been set yet
```cpp
#define MOVE_ERR_ACCELERATION_IS_UNDEFINED -3
```
### Return codes of `rampState()`

The return value is an uint8_t, which consist of two fields:

| Bit 7   | Bits 6-5  | Bits 4-0 |
|:--------|:----------|:---------|
|Always 0 | Direction | State    |

The bit mask for direction and state:
```cpp
#define RAMP_DIRECTION_MASK (32 + 64)
#define RAMP_STATE_MASK (1 + 2 + 4 + 8 + 16)
```
The defined ramp states are:
```cpp
#define RAMP_STATE_IDLE 0
#define RAMP_STATE_COAST 1
#define RAMP_STATE_ACCELERATE 2
#define RAMP_STATE_DECELERATE_TO_STOP 4
#define RAMP_STATE_DECELERATE (4 + 8)
#define RAMP_STATE_REVERSE (4 + 16)
#define RAMP_STATE_ACCELERATING_FLAG 2
#define RAMP_STATE_DECELERATING_FLAG 4
```
And the two directions of a move
```cpp
#define RAMP_DIRECTION_COUNT_UP 32
#define RAMP_DIRECTION_COUNT_DOWN 64
```

## Timing values - Architecture dependent

### AVR
|VARIABLE         | Value       | Unit                    |
|:----------------|------------:|:------------------------|
|TICKS_PER_S      | 16_000_000  | [ticks/s]               |
|MIN_CMD_TICKS    |  640        | [1/TICKS_PER_S seconds] |
|MIN_DIR_DELAY_US |   40        | [µs]                    |
|MAX_DIR_DELAY_US | 4095        | [µs]                    |

### ESP32
|VARIABLE         | Value       | Unit                    |
|:----------------|------------:|:------------------------|
|TICKS_PER_S      | 16_000_000  | [ticks/s]               |
|MIN_CMD_TICKS    | 8000        | [1/TICKS_PER_S seconds] |
|MIN_DIR_DELAY_US |  500        | [µs]                    |
|MAX_DIR_DELAY_US | 4095        | [µs]                    |

### SAM DUE
|VARIABLE         | Value       | Unit                    |
|:----------------|------------:|:------------------------|
|TICKS_PER_S      | 21_000_000  | [ticks/s]               |
|MIN_CMD_TICKS    | 4200        | [1/TICKS_PER_S seconds] |
|MIN_DIR_DELAY_US |  200        | [µs]                    |
|MAX_DIR_DELAY_US | 3120        | [µs]                    |

# FastAccelStepper
## Step Pin
step pin is defined at creation. Here can retrieve the pin
```cpp
  uint8_t getStepPin();
```
## Direction Pin
if direction pin is connected, call this function.

If the pin number is >= 128, then the direction pin is assumed to be
external and the external callback function (set by `setExternalCallForPin()`)
is used to set the pin. For direction pin, this is only implemented for esp32
and its supported derivates.

For slow driver hardware the first step after any polarity change of the
direction pin can be delayed by the value dir_change_delay_us. The allowed
range is MIN_DIR_DELAY_US and MAX_DIR_DELAY_US. The special value of 0
means, that no delay is added. Values 1 up to MIN_DIR_DELAY_US will be
clamped to MIN_DIR_DELAY_US. Values above MAX_DIR_DELAY_US will be clamped
to MAX_DIR_DELAY_US. For external pins, dir_change_delay_us is ignored,
because the mechanism applied for external pins provides already pause
in the range of ms or more.
```cpp
  void setDirectionPin(uint8_t dirPin, bool dirHighCountsUp = true,
                       uint16_t dir_change_delay_us = 0);
  uint8_t getDirectionPin() { return _dirPin; }
  bool directionPinHighCountsUp() { return _dirHighCountsUp; }
```
## Enable Pin
if enable pin is connected, then use this function.

If the pin number is >= 128, then the enable pin is assumed to be
external and the external callback function (set by `setExternalCallForPin()`)
is used to set the pin.

In case there are two enable pins: one low and one high active, then
these calls are valid and both pins will be operated:
   setEnablePin(pin1, true);
   setEnablePin(pin2, false);
If pin1 and pin2 are same, then the last call will be used.
```cpp
  void setEnablePin(uint8_t enablePin, bool low_active_enables_stepper = true);
  uint8_t getEnablePinHighActive() { return _enablePinHighActive; }
  uint8_t getEnablePinLowActive() { return _enablePinLowActive; }
```
using enableOutputs/disableOutputs the stepper can be enabled and disabled
For a running motor with autoEnable set, disableOutputs() will return false
  bool enableOutputs();returns true, if enabled
  bool disableOutputs();returns true, if disabled
In auto enable mode, the stepper is enabled before stepping and disabled
afterwards. The delay from stepper enabled till first step and from
last step to stepper disabled can be separately adjusted.
The delay from enable to first step is done in ticks and as such is limited
to MAX_ON_DELAY_TICKS, which translates approximately to 120ms for
esp32 and 60ms for avr at 16 MHz). The delay till disable is done in period
interrupt/task with 4 or 10 ms repetition rate and as such is with several
ms jitter.
```cpp
  void setAutoEnable(bool auto_enable);
  int8_t setDelayToEnable(uint32_t delay_us);
  void setDelayToDisable(uint16_t delay_ms);
#define DELAY_OK 0
#define DELAY_TOO_LOW -1
#define DELAY_TOO_HIGH -2
```
## Stepper Position
Retrieve the current position of the stepper
```cpp
  int32_t getCurrentPosition();
```
Set the current position of the stepper - either in standstill or while
moving.
   for esp32: the implementation uses getCurrentPosition(), which does not
              consider the steps of the current command
              => recommend to use only in standstill
```cpp
  void setCurrentPosition(int32_t new_pos);
```
## Stepper running status
is true while the stepper is running or ramp generation is active
```cpp
  bool isRunning();
```
## Speed
For stepper movement control by FastAccelStepper's ramp generator

Speed can be defined in four different units:
- In Hz: This means steps/s
- In millHz: This means in steps/1000s
- In us: This means in us/step

For the device's maximum allowed speed, the following calls can be used.
```cpp
  uint16_t getMaxSpeedInUs();
  uint16_t getMaxSpeedInTicks();
  uint32_t getMaxSpeedInHz();
  uint32_t getMaxSpeedInMilliHz();
```
Setting the speed can be done with the four `setSpeed...()` calls.
The new value will be used only after call of these functions:

- `move()`
- `moveTo()`
- `runForward()`
- `runBackward()`
- `applySpeedAcceleration()`
- `moveByAcceleration()`

Note: no update on `stopMove()`

Returns 0 on success, or -1 on invalid value.
Invalid is faster than MaxSpeed or slower than ~250 Mio ticks/step.
```cpp
  int8_t setSpeedInUs(uint32_t min_step_us);
  int8_t setSpeedInTicks(uint32_t min_step_ticks);
  int8_t setSpeedInHz(uint32_t speed_hz);
  int8_t setSpeedInMilliHz(uint32_t speed_mhz);
```
To retrieve current set speed. This means, while accelerating and/or
decelerating, this is NOT the actual speed !
```cpp
  uint32_t getSpeedInUs() { return _rg.getSpeedInUs(); }
  uint32_t getSpeedInTicks() { return _rg.getSpeedInTicks(); }
  uint32_t getSpeedInMilliHz() { return _rg.getSpeedInMilliHz(); }
```
If the current speed is needed, then use `getCurrentSpeed...()`. This
retrieves the actual speed.

| value | description                  |
|:-----:|:-----------------------------|
|   = 0 | while not moving             |
|   > 0 | while position counting up   |
|   < 0 | while position counting down |

```cpp
  int32_t getCurrentSpeedInUs();
  int32_t getCurrentSpeedInMilliHz();
```
## Acceleration
 set Acceleration expects as parameter the change of speed
 as step/s².
 If for example the speed should ramp up from 0 to 10000 steps/s within
 10s, then the acceleration is 10000 steps/s / 10s = 1000 steps/s²

New value will be used after call to
move/moveTo/runForward/runBackward/applySpeedAcceleration/moveByAcceleration

note: no update on stopMove()

Returns 0 on success, or -1 on invalid value (<=0)
```cpp
  int8_t setAcceleration(int32_t step_s_s) {
    return _rg.setAcceleration(step_s_s);
  }
  uint32_t getAcceleration() { return _rg.getAcceleration(); }
```
getCurrentAcceleration() retrieves the actual acceleration.
   = 0 while idle or coasting
   > 0 while speed is changing towards positive values
   < 0 while speed is changeing towards negative values
```cpp
  int32_t getCurrentAcceleration() {
    return _rg.getCurrentAcceleration();
  }
```
## Apply new speed/acceleration value
This function applies new values for speed/acceleration.
This is convenient especially, if the stepper is set to continuous running.
```cpp
  void applySpeedAcceleration();
```
## Move commands
start/move the stepper for (move) steps or to an absolute position.

If the stepper is already running, then the current running move will be
updated together with any updated values of acceleration/speed. The move is
relative to the target position of any ongoing move ! If the new
move/moveTo for an ongoing command would reverse the direction, then the
command is silently ignored.
return values are the MOVE_... constants
```cpp
  int8_t move(int32_t move, bool blocking = false);
  int8_t moveTo(int32_t position, bool blocking = false);
```
This command flags the stepper to keep run continuously into current
direction. It can be stopped by stopMove.
Be aware, if the motor is currently decelerating towards reversed
direction, then keepRunning() will speed up again and not finish direction
reversal first.
```cpp
  void keepRunning();
  bool isRunningContinuously() { return _rg.isRunningContinuously(); }
```
This command just let the motor run continuously in one direction.
If the motor is running in the opposite direction, it will reverse
return value as with move/moveTo
```cpp
  int8_t runForward();
  int8_t runBackward();
```
forwardStep()/backwardstep() can be called, while stepper is not moving
If stepper is moving, this is a no-op.
backwardStep() is a no-op, if no direction pin defined
It will immediately let the stepper perform one single step.
If blocking = true, then the routine will wait till isRunning() is false
```cpp
  void forwardStep(bool blocking = false);
  void backwardStep(bool blocking = false);
```
moveByAcceleration() can be called, if only the speed of the stepper
is of interest and that speed to be controlled by acceleration.
The maximum speed (in both directions) to be set by setSpeedInUs() before.
The behaviour will be:
   acceleration > 0  => accelerate towards positive maximum speed
   acceleration = 0  => keep current speed
   acceleration < 0
       => accelerate towards negative maximum speed if allow_reverse
       => decelerate towards motor stop if allow_reverse = false
return value as with move/moveTo
```cpp
  int8_t moveByAcceleration(int32_t acceleration, bool allow_reverse = true);
```
stop the running stepper with normal deceleration.
This only sets a flag and can be called from an interrupt !
```cpp
  void stopMove();
  bool isStopping() { return _rg.isStopping(); }
```
abruptly stop the running stepper without deceleration.
This can be called from an interrupt !

The stepper command queue will be processed, but no furter commands are
added. This means, that the stepper can be expected to stop within approx.
20ms.
```cpp
  void forceStop();
```
abruptly stop the running stepper without deceleration.
This can be called from an interrupt !

No further step will be issued. As this is aborting all commands in the
queue, the actual stop position is lost (recovering this position cannot be
done within an interrupt). So the new position after stop has to be
provided and will be set as current position after stop.
```cpp
  void forceStopAndNewPosition(uint32_t new_pos);
```
get the target position for the current move
```cpp
  int32_t targetPos() { return _rg.targetPosition(); }
```
## Low Level Stepper Queue Management (low level access)

If the queue is already running, then the start parameter is obsolote.
But the queue may run out of commands while executing addQueueEntry,
so it is better to set start=true to automatically restart/continue
a running queue.

If the queue is not running, then the start parameter defines starting it
or not. The latter case is of interest to first fill the queue and then
start it.

The call addQueueEntry(NULL, true) just starts the queue. This is intended
to achieve a near synchronous start of several steppers. Consequently it
should be called with interrupts disabled and return very fast.
Actually this is necessary, too, in case the queue is full and not
started.
```cpp
  int8_t addQueueEntry(const struct stepper_command_s* cmd, bool start = true);
```
Return codes for addQueueEntry
   positive values mean, that caller should retry later
```cpp
#define AQE_OK 0
#define AQE_QUEUE_FULL 1
#define AQE_DIR_PIN_IS_BUSY 2
#define AQE_WAIT_FOR_ENABLE_PIN_ACTIVE 3
#define AQE_DEVICE_NOT_READY 4
#define AQE_ERROR_TICKS_TOO_LOW -1
#define AQE_ERROR_EMPTY_QUEUE_TO_START -2
#define AQE_ERROR_NO_DIR_PIN_TO_TOGGLE -3
```
### check functions for command queue being empty, full or running.
```cpp
  bool isQueueEmpty();
  bool isQueueFull();
  bool isQueueRunning();
```
### functions to get the fill level of the queue

To retrieve the forward planning time in the queue, ticksInQueue()
can be used. It sums up all ticks of the not yet processed commands.
For commands defining pauses, the summed up value is entry.ticks.
For commands with steps, the summed up value is entry.steps*entry.ticks
```cpp
  uint32_t ticksInQueue();
```
This function can be used to check, if the commands in the queue
will last for <min_ticks> ticks. This is again without the
currently processed command.
```cpp
  bool hasTicksInQueue(uint32_t min_ticks);
```
This function allows to check the number of commands in the queue.
This is including the currently processed command.
```cpp
  uint8_t queueEntries();
```
Get the future position of the stepper after all commands in queue are
completed
```cpp
  int32_t getPositionAfterCommandsCompleted();
```
Get the future speed of the stepper after all commands in queue are
completed. This is in µs. Returns 0 for stopped motor

This value comes from the ramp generator and is not valid for raw command
queue
==> Will be renamed in future release
```cpp
  uint32_t getPeriodInUsAfterCommandsCompleted();
  uint32_t getPeriodInTicksAfterCommandsCompleted();
```
Set the future position of the stepper after all commands in queue are
completed. This has immediate effect to getCurrentPosition().
```cpp
  void setPositionAfterCommandsCompleted(int32_t new_pos);
```
This function provides info, in which state the high level stepper control
is operating. The return value is an `or` of RAMP_STATE_... and
RAMP_DIRECTION_... flags. Definitions are above
```cpp
  uint8_t rampState() { return _rg.rampState(); }
```
returns true, if the ramp generation is active
```cpp
  bool isRampGeneratorActive() { return _rg.isRampGeneratorActive(); }
```
These functions allow to detach and reAttach a step pin for other use.
Pretty low level, use with care or not at all
```cpp
  void detachFromPin();
  void reAttachToPin();
```
## ESP32 only: Free pulse counter
These two functions are only available on esp32.
The first can attach any of the eight pulse counters to this stepper.
The second then will read the current pulse counter value

The user is responsible to not use a pulse counter, which is occupied by a
stepper and by this render the stepper or even blow up the uC.

Pulse counter 6 and 7 are not used by the stepper library and are judged as
available. If only five steppers are defined, then 5 gets available. If
four steppers are defined, then 4 is usable,too.

These functions are intended primarily for testing, because the library
should always output the correct amount of pulses. Possible application
usage would be an immediate and interrupt friendly version for
getCurrentPosition()

The pulse counter counts up towards high_value.
If the high_value is reached, then the pulse counter is reset to 0.
Similarly, if direction pin is configured and set to count down,
then the pulse counter counts towards low_value. When the low value is hit,
the pulse counter is reset to 0.

If low_value and high_value is set 0 zero, then the pulse counter is just
counting like any int16_t counter: 0...32767,-32768,-32767,...,0 and
backwards accordingly

Possible application:
Assume the stepper, to which the pulse counter attached to, needs 3200
steps/revolution. If now attachToPulseCounter is called with -3200 and 3200
for the low and high values respectively, then the momentary angle of the
stepper (at exact this moment) can be retrieved just by reading the pulse
counter. If the value is negative, then just add 3200.

```cpp
  bool attachToPulseCounter(uint8_t pcnt_unit, int16_t low_value = -16384,
                            int16_t high_value = 16384);
  int16_t readPulseCounter();
  void clearPulseCounter();
  bool pulseCounterAttached() { return _attached_pulse_cnt_unit >= 0; }
#endif
```
