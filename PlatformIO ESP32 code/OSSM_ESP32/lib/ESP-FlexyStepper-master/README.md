# ESP-FlexyStepper

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/478b7cf581664442bc75b6a87b645553)](https://app.codacy.com/manual/pkerspe/ESP-FlexyStepper?utm_source=github.com&utm_medium=referral&utm_content=pkerspe/ESP-FlexyStepper&utm_campaign=Badge_Grade_Dashboard)

This library is used to control one or more stepper motors with a ESP 32 module. The motors are accelerated and decelerated as they travel to their destination. The library has been optimized for flexible control where speeds and positions can be changed while in-motion. Based on S.Reifels FlexyStepper library.

## Features

The library provides the following features:
  - generating pulses for a connected stepper driver with a dir and step input
  - connection of emergency switch to stop all motion immendiately
  - connection of limit switches / homing switches
  - blocking and non blocking function calls possible
  - callback functions to handle events like position reached, homing complete etc.
  - can run in different modes:
    - as a service / task in the background (so you can do whatever you want in the main loop of your sketch without interfering with the stepper motion)
    - manually call the processMovement() function in the main loop (then you have to make sure your main loop completes quick enough to ensure smooth movement
    - use the blocking movement functions, that take care of calling processMovement but block the main loop for the duration of the movement

## Example

The following is an example of how to use the library as a service running in the "background" as a separate Task on the ESP32:

```#include <ESP_FlexyStepper.h>

// IO pin assignments
const int MOTOR_STEP_PIN = 33;
const int MOTOR_DIRECTION_PIN = 25;
const int EMERGENCY_STOP_PIN = 13; //define the IO pin the emergency stop switch is connected to
const int LIMIT_SWITCH_PIN = 32;   //define the IO pin where the limit switches are connected to (switches in series in normally closed setup against ground)

// Speed settings
const int DISTANCE_TO_TRAVEL_IN_STEPS = 2000;
const int SPEED_IN_STEPS_PER_SECOND = 300;
const int ACCELERATION_IN_STEPS_PER_SECOND = 800;
const int DECELERATION_IN_STEPS_PER_SECOND = 800;

// create the stepper motor object
ESP_FlexyStepper stepper;

int previousDirection = 1;

void setup()
{
  Serial.begin(115200);
  // connect and configure the stepper motor to its IO pins
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);
  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepper.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);
  
  // Not start the stepper instance as a service in the "background" as a separate task
  // and the OS of the ESP will take care of invoking the processMovement() task regularily so you can do whatever you want in the loop function
  stepper.startAsService();
}

void loop()
{
  // just move the stepper back and forth in an endless loop
  if (stepper.getDistanceToTargetSigned() == 0)
  {
    delay(5000);
    previousDirection *= -1;
    long relativeTargetPosition = DISTANCE_TO_TRAVEL_IN_STEPS * previousDirection;
    Serial.printf("Moving stepper by %ld steps\n", relativeTargetPosition);
    stepper.setTargetPositionRelativeInSteps(relativeTargetPosition);
  }
  
  // Notice that you can now do whatever you want in the loop function without the need to call processMovement().
  // also you do not have to care if your loop processing times are too long. 
}
```

## Function overview

| Function | Desciption |
| --- | --- |
| `ESP_FlexyStepper()` | constructor for the class to create a new instance of the ESP-FlexyStepper |
| `void startAsService()` | start ESP-FlexyStepper as a seprate task (service) in the background so it handles the calls to processMovement() for you in thebackground and you are free to do whatever you want in the main loop. *Should NOT be used in combination with the synchronous (blocking) function calls for movement* |
| `void stopService()` | stop the ESP-FlexyStepper service. Only needed if startAsService() has been called before |
| `void connectToPins(byte stepPinNumber, byte directionPinNumber)` | setup the pin numbers where the external stepper driver is connected to. Provide the IO Pins fro step (or pulse) pin and direction pin |
| `bool isStartedAsService()` | returns true if the ESP-FlexyStepper service has been started/is running, false if not |
| `void setStepsPerMillimeter(float motorStepPerMillimeter)` | Configure the amount of steps (pulses) to the stepper driver that are needed to peform a movement of 1 mm |
| `void setStepsPerRevolution(float motorStepPerRevolution)` | Configure the amount of steps (pulses) to the stepper driver that are needed to peform a full rotation of the stepper motor |
| `void setDirectionToHome(signed char directionTowardHome)` | configure the direction in which to move to reach the home switch |
| `void setAccelerationInMillimetersPerSecondPerSecond(float accelerationInMillimetersPerSecondPerSecond)` | |
| `void setDecelerationInMillimetersPerSecondPerSecond(float decelerationInMillimetersPerSecondPerSecond)` | |
| `float getCurrentPositionInMillimeters()` | get the current, absolute position in milimeters. Requires that the library has been configrued properly by using the `setStepsPerMillimeter(...)` function |
| `void setCurrentPositionInMillimeters(float currentPositionInMillimeters)` | set the register for the current position to a specific value e.g. to mark the home position |
| `void setSpeedInMillimetersPerSecond(float speedInMillimetersPerSecond)` | set the speed for the next movements (or the current motion if any is in progress) in mm/s. Requires prior configuration of the library using `setStepsPerMillimeter()` |
| `bool moveToHomeInMillimeters(signed char directionTowardHome, float speedInMillimetersPerSecond, long maxDistanceToMoveInMillimeters, int homeLimitSwitchPin)` | |
| `void setSpeedInRevolutionsPerSecond(float speedInRevolutionsPerSecond)` | set the speed for the next movements (or the current motion if any is in progress) in revs/s. Requires prior configuration of the library using `setStepsPerRevolution()`|
| `void setSpeedInStepsPerSecond(float speedInStepsPerSecond)` | set the speed for the next movements (or the current motion if any is in progress) in revs/s. Requires prior configuration of the library using `setStepsPerMilimeter()` |
| `void setAccelerationInRevolutionsPerSecondPerSecond(float accelerationInRevolutionsPerSecondPerSecond)` | configure the acceleration in revs/second/second |
| `void setDecelerationInRevolutionsPerSecondPerSecond(float decelerationInRevolutionsPerSecondPerSecond)` | configure the deceleration in revs/second/second |
| `void setAccelerationInStepsPerSecondPerSecond(float accelerationInStepsPerSecondPerSecond)` | configure the acceleration in steps/second/second |
| `void setDecelerationInStepsPerSecondPerSecond(float decelerationInStepsPerSecondPerSecond)` | configure the deceleration in steps/second/second |
| `void moveRelativeInMillimeters(float distanceToMoveInMillimeters)` | *Blocking call:* start relative movement. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionRelativeInMillimeters(float distanceToMoveInMillimeters)` | set the target position to a relative value in mm from the current position. Requires the repeated call of processMovement() to sequentially update the stepper position or you need to start the ESP-FlexyStepper as as service using `startAsService()` and let the library to the rest for you. |
| `void moveToPositionInMillimeters(float absolutePositionToMoveToInMillimeters)` | *Blocking call:* start movement to absolute position in mm. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionInMillimeters(float absolutePositionToMoveToInMillimeters)` | set the target position to an absolute value in mm. Requires the repeated call of processMovement() to sequentially update the stepper position or you need to start the ESP-FlexyStepper as as service using `startAsService()` and let the library to the rest for you. |
| `long getDistanceToTargetSigned(void)` | get the distance in steps to travel from the current position to the target position. If stepper has reached it's target position then 0 will be returned. This is a signed value, depending on the direction of the current movement |
| `void setCurrentPositionInRevolutions(float currentPositionInRevolutions)` | set the current position inr revolutions (basically assign a value to the current position) |
| `float getCurrentPositionInRevolutions()` | get the current, absolute position in revs |
| `bool moveToHomeInRevolutions(signed char directionTowardHome, float speedInRevolutionsPerSecond, long maxDistanceToMoveInRevolutions, int homeLimitSwitchPin)` | *Blocking call:* move to home position (max steps or until limit switch goes low. This is a blocking function, it will not return before the final position has been reached. |
| `void moveRelativeInRevolutions(float distanceToMoveInRevolutions)` | *Blocking call:* start relative movement with given number of revolutions. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionRelativeInRevolutions(float distanceToMoveInRevolutions)` | set the target position to a relative value in revolutions from the current position. Requires the repeated call of processMovement() to sequentially update the stepper position or you need to start the ESP-FlexyStepper as as service using `startAsService()` and let the library to the rest for you. |
| `void moveToPositionInRevolutions(float absolutePositionToMoveToInRevolutions)` | *Blocking call:* start absolute movement in revolutions. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionInRevolutions(float absolutePositionToMoveToInRevolutions)` | set the target position to an absolute value in revolutions from the current position. Requires the repeated call of processMovement() to sequentially update the stepper position or you need to start the ESP-FlexyStepper as as service using `startAsService()` and let the library to the rest for you. |
| `float getCurrentVelocityInRevolutionsPerSecond()` | return the current velocity as floating point number in Revolutions/Second *Note: make sure you configured the stepper correctly using the `setStepsPerRevolution` function before calling this function, otherwise the result might be incorrect!*|
| `float getCurrentVelocityInStepsPerSecond()` | return the current velocity as floating point number in Steps/Second |
| `float getCurrentVelocityInMillimetersPerSecond(void)` | return the current velocity as floating point number in milimeters/Second. *Note: make sure you configured the stepper correctly using the `setStepsPerMillimeter` function before calling this function, otherwise the result might be incorrect!* |
| `void setCurrentPositionInSteps(long currentPositionInSteps)` |  |
| `void setCurrentPositionAsHomeAndStop(void)` | set the current position of the stepper as the home position. This also sets the current position to 0. After peforming this step you can always return to the home position by calling `setTargetPoisitionInSteps(0)`(or for blocking calls `moveToPositionInSteps(0)`) |
| `long getCurrentPositionInSteps()` | return the current position of the stepper in steps (absolute value, could also be negative if no proper homing has been performed before) |
| `bool moveToHomeInSteps(signed char directionTowardHome, float speedInStepsPerSecond, long maxDistanceToMoveInSteps, int homeSwitchPin)` | *Blocking call:* start movement in the given direction with a maximum number of steps or until the IO Pin defined by homeSwitchPin is going low (active low switch is required, since the library will configure this pin as input with internal pullup in the current version). This is a blocking function, it will not return before the final position has been reached.|
| `void moveRelativeInSteps(long distanceToMoveInSteps)` | *Blocking call:* start movement to the given relative position from current position. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionRelativeInSteps(long distanceToMoveInSteps)` | set a new, relative target position for the stepper in a non blocking way. Requires the repeated call of processMovement() to sequentially update the stepper position or you need to start the ESP-FlexyStepper as as service using `startAsService()` and let the library to the rest for you.|
| `void moveToPositionInSteps(long absolutePositionToMoveToInSteps)` | *Blocking call:* start movement to the given absolute position in steps. This is a blocking function, it will not return before the final position has been reached. |
| `void setTargetPositionInSteps(long absolutePositionToMoveToInSteps)` | set a new absolute target position in steps to move to |
| `void setTargetPositionToStop()` | start decelerating from the current position. Used to stop the current motion without peforming a hard stop. Does nothing if the target position has already been reached and the stepper has come to a stop |
| `void setLimitSwitchActive(byte limitSwitchType)` | externally trigger a limit switch acitvation |
| `bool motionComplete()` | returns true if the target position has been reached and the motion stopped, false if the stepper is still moving |
| `bool processMovement(void)` | calculate when the next pulse needs to be send and control high/low state of the dir and pulse/step pin. *This function does not need to be called a.) when you started the ESP-FlexyStepper as a service using the startAsService() function, b.) when you called one of the blocking/synchronous movving functions* |
| `int getDirectionOfMotion(void)` | get the direction of the current movement. 0 if stepper not moving at the moment, 1 or -1 if the stepper is in motion |
| `bool isMovingTowardsHome(void)` | true if the stepper is still on the way to the home position |
| `static void taskRunner(void *parameter)` | this is the function that is used as the service, you do not need to call this manually ever |
| `getTaskStackHighWaterMark(void)` | this is used for debugging to see if the allocated stack trace of the task / service function is large enough. You can ignore this function |
