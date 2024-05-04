
//      ******************************************************************
//      *                                                                *
//      *                    ESP-FlexyStepper                            *
//      *                                                                *
//      *            Paul Kerspe                     4.6.2020            *
//      *       based on the concept of FlexyStepper by Stan Reifel      *
//      *                                                                *
//      ******************************************************************

// MIT License
//
// Copyright (c) 2020 Paul Kerspe
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is furnished
// to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

//
// This library is used to control one or more stepper motors.  It requires a
// stepper driver board that has a Step and Direction interface.  The motors are
// accelerated and decelerated as they travel to the final position.  This driver
// supports changing the target position, speed or rate of acceleration while a
// motion is in progress.
//
// for more details and a manual on how to use it, check the README.md on github and the provided examples
// https://github.com/pkerspe/ESP-FlexyStepper/blob/master/README.md
//
// This library is based on the works of Stan Reifel in his FlexyStepper library:
// https://github.com/Stan-Reifel/FlexyStepper
//

#include "ESP_FlexyStepper.h"

//
// direction signal level for "step and direction"
//
#define POSITIVE_DIRECTION LOW
#define NEGATIVE_DIRECTION HIGH

// ---------------------------------------------------------------------------------
//                                  Setup functions
// ---------------------------------------------------------------------------------

//
// constructor for the stepper class
//
ESP_FlexyStepper::ESP_FlexyStepper()
{
  this->lastStepTime_InUS = 0L;
  this->stepsPerRevolution = 200L;
  this->stepsPerMillimeter = 25.0;
  this->directionOfMotion = 0;
  this->currentPosition_InSteps = 0L;
  this->targetPosition_InSteps = 0L;
  this->setSpeedInStepsPerSecond(200);
  this->setAccelerationInStepsPerSecondPerSecond(200.0);
  this->setDecelerationInStepsPerSecondPerSecond(200.0);
  this->currentStepPeriod_InUS = 0.0;
  this->nextStepPeriod_InUS = 0.0;
  this->emergencyStopActive = false;
  this->holdEmergencyStopUntilExplicitRelease = false;
  this->isCurrentlyHomed = false;
  this->directionTowardsHome = -1;
  this->disallowedDirection = 0;
  this->activeLimitSwitch = 0; //see LIMIT_SWITCH_BEGIN and LIMIT_SWITCH_END
  this->lastStepDirectionBeforeLimitSwitchTrigger = 0;
  this->limitSwitchCheckPeformed = false;
}

ESP_FlexyStepper::~ESP_FlexyStepper()
{
  if (this->xHandle != NULL)
  {
    this->stopService();
  }
}

//TODO: use https://github.com/nrwiersma/ESP8266Scheduler/blob/master/examples/simple/simple.ino for ESP8266
void ESP_FlexyStepper::startAsService(void)
{
  disableCore1WDT(); // we have to disable the Watchdog timer to prevent it from rebooting the ESP all the time another option would be to add a vTaskDelay but it would slow down the stepper
  xTaskCreatePinnedToCore(
      ESP_FlexyStepper::taskRunner, /* Task function. */
      "FlexyStepper",               /* String with name of task (by default max 16 characters long) */
      2000,                         /* Stack size in bytes. */
      this,                         /* Parameter passed as input of the task */
      1,                            /* Priority of the task, 1 seems to work just fine for us */
      &this->xHandle,               /* Task handle. */
      1);              
}

void ESP_FlexyStepper::taskRunner(void *parameter)
{
  ESP_FlexyStepper *stepperRef = static_cast<ESP_FlexyStepper *>(parameter);
  for (;;)
  {
    stepperRef->processMovement();
    //vTaskDelay(1); // This would be a working solution to prevent the WDT to fire (if not disabled, yet it will cause noticeably less smooth stepper movements / lower frequencies)
  }
}

void ESP_FlexyStepper::stopService(void)
{
  vTaskDelete(this->xHandle);
  this->xHandle = NULL;
}

bool ESP_FlexyStepper::isStartedAsService()
{
  return (this->xHandle != NULL);
}

/**
 * get the overall max stack size since task creation (since the call to startAsService() )
 * This function is used to determine if the stacksize is large enough and has more of a debugging purpose.
 * Return the minimum amount of free bytes on the stack that has been measured so far.
 */
long ESP_FlexyStepper::getTaskStackHighWaterMark()
{
  if (this->isStartedAsService())
  {
    return uxTaskGetStackHighWaterMark(this->xHandle);
  }
  return 0;
}

/**
 * get the distance in steps to the currently set target position.
 * 0 is returned if the stepper is already at the target position.
 * The returned value is signed, depending on the direction to move to reach the target
 */
long ESP_FlexyStepper::getDistanceToTargetSigned()
{
  return (this->targetPosition_InSteps - this->currentPosition_InSteps);
}

/**
 * perform an emergency stop, causing all movements to be canceled instantly
 * the optional parameter 'holdUntilReleased' allows to define if the emergency stop shall only affect the current motion (if any) 
 * or if it should hold the emergency stop status (kind of a latching functionality) until the releaseEmergencyStop() function is called explicitly.
 * Default for holdUntilReleased is false (if paremter is ommitted)
 */
void ESP_FlexyStepper::emergencyStop(bool holdUntilReleased)
{
  this->holdEmergencyStopUntilExplicitRelease = holdUntilReleased;
  this->emergencyStopActive = (!this->motionComplete() || this->holdEmergencyStopUntilExplicitRelease);
  if (this->_emergencyStopTriggeredCallback)
  {
    this->_emergencyStopTriggeredCallback();
  }
}

/**
 * releases an emergency stop that has previously been engaded using a call to emergencyStop(true)
 */
void ESP_FlexyStepper::releaseEmergencyStop()
{
  this->emergencyStopActive = false;
  if (this->_emergencyStopReleasedCallback)
  {
    this->_emergencyStopReleasedCallback();
  }
}

/**
 *  configure the direction in which to move to reach the home position
 *  Accepts 1 or -1 as allowed values. Other values will be ignored
 */
void ESP_FlexyStepper::setDirectionToHome(signed char directionTowardHome)
{
  if (directionTowardHome == -1 || directionTowardHome == 1)
  {
    this->directionTowardsHome = directionTowardHome;
  }
}

/**
 * Notification of an externaly detected limit switch activation
 * Accepts LIMIT_SWITCH_BEGIN (-1) or LIMIT_SWITCH_END as parameter values to indicate 
 * whether the limit switch near the begin (direction of home position) or at the end of the movement has ben triggered.
 * It is strongly recommended to perform debouncing before calling this function to prevent issues when button is released and retriggering the limit switch function
 */
void ESP_FlexyStepper::setLimitSwitchActive(byte limitSwitchType)
{
  if (limitSwitchType == LIMIT_SWITCH_BEGIN || limitSwitchType == LIMIT_SWITCH_END || limitSwitchType == LIMIT_SWITCH_COMBINED_BEGIN_AND_END)
  {
    this->activeLimitSwitch = limitSwitchType;
    this->limitSwitchCheckPeformed = false; //set flag for newly set limit switch trigger
    if (this->_limitTriggeredCallback)
    {
      this->_limitTriggeredCallback();
    }
  }
}

/**
 * clear the limit switch flag to allow movement in both directions again
 */
void ESP_FlexyStepper::clearLimitSwitchActive()
{
  this->activeLimitSwitch = 0;
}

/**
 * get the current direction of motion of the connected stepper motor
 * returns 1 for "forward" motion
 * returns -1 for "backward" motion
 * returns 0 if the stepper has reached its destination position and is not moving anymore
 */
int ESP_FlexyStepper::getDirectionOfMotion(void)
{
  return this->directionOfMotion;
}

/**
 * returns true if the stepper is currently in motion and moving in the direction of the home position.
 * Depends on the settings of setDirectionToHome() which defines where "home" is...a rather philosophical question :-)
 */
bool ESP_FlexyStepper::isMovingTowardsHome()
{
  return (this->directionOfMotion == this->directionTowardsHome);
}

/*
* connect the stepper object to the IO pins
* stepPinNumber = IO pin number for the Step signale
* directionPinNumber = IO pin number for the direction signal
*/
void ESP_FlexyStepper::connectToPins(byte stepPinNumber, byte directionPinNumber)
{
  this->stepPin = stepPinNumber;
  this->directionPin = directionPinNumber;

  // configure the IO pins
  pinMode(stepPin, OUTPUT);
  digitalWrite(stepPin, LOW);
  
  if(directionPin<255){
	pinMode(directionPin, OUTPUT);
	}
  digitalWrite(directionPin, LOW);
}

/*
* setup an IO pin to trigger an external brake for the motor.
* This is an optional step, set to -1 to disable this function (which is default)
* the active state parameter defines if the external brake is configured in an active high (pin goes high to enable the brake) or active low (pin goes low to activate the brake) setup.
* active high = 1, active low = 2
* Will be set to ative high by default or if an invalid value is given
*/
void ESP_FlexyStepper::setBrakePin(byte brakePin, byte activeState)
{
  this->brakePin = brakePin;
  if (activeState == ESP_FlexyStepper::ACTIVE_HIGH || activeState == ESP_FlexyStepper::ACTIVE_LOW)
  {
    this->brakePinActiveState = activeState;
  }
  else
  {
    this->brakePinActiveState = ESP_FlexyStepper::ACTIVE_HIGH;
  }

  if (this->brakePin >= 0)
  {
    // configure the IO pins
    pinMode(this->brakePin, OUTPUT);
    this->deactivateBrake();
    _isBrakeConfigured = true;
  }
  else
  {
    _isBrakeConfigured = false;
  }
}

/**
 * set a delay in milliseconds between stopping the stepper motor and engaging the pyhsical brake (trigger the eternal pin configured via setBrakePin() ).
 * Default is 0, resulting in immediate triggering of the motor brake once the motor stops moving.
 * This value does NOT affect the triggering of the brake in cade of an emergency stop. In this case the brake will always get triggered without delay
 */
void ESP_FlexyStepper::setBrakeEngageDelayMs(unsigned long delay)
{
  this->_brakeEngageDelayMs = delay;
}

/**
 * set a timeout in milliseconds fter which the brake shall be released once triggered and no motion is performed by the stpper motor.
 * By default the value is -1 indicating, that the brake shall never be automatically released, as long as the stepper motor is not moving to a new position.
 * Value must be larger than 1 (Even though 1ms delay does probably not make any sense since physical brakes have a delay that is most likely higher than that just to engange)
 */
void ESP_FlexyStepper::setBrakeReleaseDelayMs(signed long delay)
{
  if (delay < 0)
  {
    this->_brakeReleaseDelayMs = -1;
  }
  else
  {
    this->_brakeReleaseDelayMs = delay;
  }
}

/**
 * activate (engage) the motor brake (if any is configured, otherwise will do nothing) 
 */
void ESP_FlexyStepper::activateBrake()
{
  if (this->_isBrakeConfigured)
  {
    digitalWrite(this->brakePin, (this->brakePinActiveState == ESP_FlexyStepper::ACTIVE_HIGH) ? 1 : 0);
    this->_isBrakeActive = true;
    this->_timeToEngangeBrake = LONG_MAX;
  }
}

/**
 * deactivate (release) the motor brake (if any is configured, otherwise will do nothing) 
 */
void ESP_FlexyStepper::deactivateBrake()
{
  if (this->_isBrakeConfigured)
  {
    digitalWrite(this->brakePin, (this->brakePinActiveState == ESP_FlexyStepper::ACTIVE_HIGH) ? 0 : 1);
    this->_isBrakeActive = false;
    this->_timeToReleaseBrake = LONG_MAX;
    this->_hasMovementOccuredSinceLastBrakeRelease = false;
  }
}

bool ESP_FlexyStepper::isBakeActive()
{
  return this->_isBrakeActive;
}

// ---------------------------------------------------------------------------------
//                     Public functions with units in millimeters
// ---------------------------------------------------------------------------------

//
// set the number of steps the motor has per millimeters
//
void ESP_FlexyStepper::setStepsPerMillimeter(float motorStepsPerMillimeter)
{
  stepsPerMillimeter = motorStepsPerMillimeter;
}

//
// get the current position of the motor in millimeters, this functions is updated
// while the motor moves
//  Exit:  a signed motor position in millimeters returned
//
float ESP_FlexyStepper::getCurrentPositionInMillimeters()
{
  return ((float)getCurrentPositionInSteps() / stepsPerMillimeter);
}

//
// set the current position of the motor in millimeters, this does not move the
// motor
//
void ESP_FlexyStepper::setCurrentPositionInMillimeters(
    float currentPositionInMillimeters)
{
  setCurrentPositionInSteps((long)round(currentPositionInMillimeters *
                                        stepsPerMillimeter));
}

//
// set the maximum speed, units in millimeters/second, this is the maximum speed
// reached while accelerating
//  Enter:  speedInMillimetersPerSecond = speed to accelerate up to, units in
//            millimeters/second
//
void ESP_FlexyStepper::setSpeedInMillimetersPerSecond(float speedInMillimetersPerSecond)
{
  setSpeedInStepsPerSecond(speedInMillimetersPerSecond * stepsPerMillimeter);
}

//
// set the rate of acceleration, units in millimeters/second/second
//  Enter:  accelerationInMillimetersPerSecondPerSecond = rate of acceleration,
//          units in millimeters/second/second
//
void ESP_FlexyStepper::setAccelerationInMillimetersPerSecondPerSecond(
    float accelerationInMillimetersPerSecondPerSecond)
{
  setAccelerationInStepsPerSecondPerSecond(
      accelerationInMillimetersPerSecondPerSecond * stepsPerMillimeter);
}

//
// set the rate of deceleration, units in millimeters/second/second
//  Enter:  decelerationInMillimetersPerSecondPerSecond = rate of deceleration,
//          units in millimeters/second/second
//
void ESP_FlexyStepper::setDecelerationInMillimetersPerSecondPerSecond(
    float decelerationInMillimetersPerSecondPerSecond)
{
  setDecelerationInStepsPerSecondPerSecond(
      decelerationInMillimetersPerSecondPerSecond * stepsPerMillimeter);
}

//
// home the motor by moving until the homing sensor is activated, then set the
// position to zero, with units in millimeters
//  Enter:  directionTowardHome = 1 to move in a positive direction, -1 to move
//            in a negative directions
//          speedInMillimetersPerSecond = speed to accelerate up to while moving
//            toward home, units in millimeters/second
//          maxDistanceToMoveInMillimeters = unsigned maximum distance to move
//            toward home before giving up
//          homeSwitchPin = pin number of the home switch, switch should be
//            configured to go low when at home
//  Exit:   true returned if successful, else false
//
bool ESP_FlexyStepper::moveToHomeInMillimeters(signed char directionTowardHome,
                                               float speedInMillimetersPerSecond, long maxDistanceToMoveInMillimeters,
                                               int homeLimitSwitchPin)
{
  return (moveToHomeInSteps(directionTowardHome,
                            speedInMillimetersPerSecond * stepsPerMillimeter,
                            maxDistanceToMoveInMillimeters * stepsPerMillimeter,
                            homeLimitSwitchPin));
}

//
// move relative to the current position, units are in millimeters, this function
// does not return until the move is complete
//  Enter:  distanceToMoveInMillimeters = signed distance to move relative to the
//          current position in millimeters
//
void ESP_FlexyStepper::moveRelativeInMillimeters(float distanceToMoveInMillimeters)
{
  setTargetPositionRelativeInMillimeters(distanceToMoveInMillimeters);

  while (!processMovement())
    ;
}

//
// setup a move relative to the current position, units are in millimeters, no
// motion occurs until processMove() is called
//  Enter:  distanceToMoveInMillimeters = signed distance to move relative to the
//          current position in millimeters
//
void ESP_FlexyStepper::setTargetPositionRelativeInMillimeters(
    float distanceToMoveInMillimeters)
{
  setTargetPositionRelativeInSteps((long)round(distanceToMoveInMillimeters *
                                               stepsPerMillimeter));
}

//
// move to the given absolute position, units are in millimeters, this function
// does not return until the move is complete
//  Enter:  absolutePositionToMoveToInMillimeters = signed absolute position to
//          move to in units of millimeters
//
void ESP_FlexyStepper::moveToPositionInMillimeters(
    float absolutePositionToMoveToInMillimeters)
{
  setTargetPositionInMillimeters(absolutePositionToMoveToInMillimeters);

  while (!processMovement())
    ;
}

//
// setup a move, units are in millimeters, no motion occurs until processMove()
// is called
//  Enter:  absolutePositionToMoveToInMillimeters = signed absolute position to
//          move to in units of millimeters
//
void ESP_FlexyStepper::setTargetPositionInMillimeters(
    float absolutePositionToMoveToInMillimeters)
{
  setTargetPositionInSteps((long)round(absolutePositionToMoveToInMillimeters *
                                       stepsPerMillimeter));
}

float ESP_FlexyStepper::getTargetPositionInMillimeters()
{
	return getTargetPositionInSteps() / stepsPerMillimeter;
}

//
// Get the current velocity of the motor in millimeters/second.  This functions is
// updated while it accelerates up and down in speed.  This is not the desired
// speed, but the speed the motor should be moving at the time the function is
// called.  This is a signed value and is negative when the motor is moving
// backwards.  Note: This speed will be incorrect if the desired velocity is set
// faster than this library can generate steps, or if the load on the motor is too
// great for the amount of torque that it can generate.
//  Exit:  velocity speed in steps per second returned, signed
//
float ESP_FlexyStepper::getCurrentVelocityInMillimetersPerSecond()
{
  return (getCurrentVelocityInStepsPerSecond() / stepsPerMillimeter);
}

// ---------------------------------------------------------------------------------
//                     Public functions with units in revolutions
// ---------------------------------------------------------------------------------

//
// set the number of steps the motor has per revolution
//
void ESP_FlexyStepper::setStepsPerRevolution(float motorStepPerRevolution)
{
  stepsPerRevolution = motorStepPerRevolution;
}

//
// get the current position of the motor in revolutions, this functions is updated
// while the motor moves
//  Exit:  a signed motor position in revolutions returned
//
float ESP_FlexyStepper::getCurrentPositionInRevolutions()
{
  return ((float)getCurrentPositionInSteps() / stepsPerRevolution);
}

//
// set the current position of the motor in revolutions, this does not move the
// motor
//
void ESP_FlexyStepper::setCurrentPositionInRevolutions(
    float currentPositionInRevolutions)
{
  setCurrentPositionInSteps((long)round(currentPositionInRevolutions *
                                        stepsPerRevolution));
}

//
// set the maximum speed, units in revolutions/second, this is the maximum speed
// reached while accelerating
//  Enter:  speedInRevolutionsPerSecond = speed to accelerate up to, units in
//            revolutions/second
//
void ESP_FlexyStepper::setSpeedInRevolutionsPerSecond(float speedInRevolutionsPerSecond)
{
  setSpeedInStepsPerSecond(speedInRevolutionsPerSecond * stepsPerRevolution);
}

//
// set the rate of acceleration, units in revolutions/second/second
//  Enter:  accelerationInRevolutionsPerSecondPerSecond = rate of acceleration,
//          units in revolutions/second/second
//
void ESP_FlexyStepper::setAccelerationInRevolutionsPerSecondPerSecond(
    float accelerationInRevolutionsPerSecondPerSecond)
{
  setAccelerationInStepsPerSecondPerSecond(
      accelerationInRevolutionsPerSecondPerSecond * stepsPerRevolution);
}

//
// set the rate of deceleration, units in revolutions/second/second
//  Enter:  decelerationInRevolutionsPerSecondPerSecond = rate of deceleration,
//          units in revolutions/second/second
//
void ESP_FlexyStepper::setDecelerationInRevolutionsPerSecondPerSecond(
    float decelerationInRevolutionsPerSecondPerSecond)
{
  setDecelerationInStepsPerSecondPerSecond(
      decelerationInRevolutionsPerSecondPerSecond * stepsPerRevolution);
}

//
// home the motor by moving until the homing sensor is activated, then set the
//  position to zero, with units in revolutions
//  Enter:  directionTowardHome = 1 to move in a positive direction, -1 to move in
//            a negative directions
//          speedInRevolutionsPerSecond = speed to accelerate up to while moving
//            toward home, units in revolutions/second
//          maxDistanceToMoveInRevolutions = unsigned maximum distance to move
//            toward home before giving up
//          homeSwitchPin = pin number of the home switch, switch should be
//            configured to go low when at home
//  Exit:   true returned if successful, else false
//
bool ESP_FlexyStepper::moveToHomeInRevolutions(signed char directionTowardHome,
                                               float speedInRevolutionsPerSecond, long maxDistanceToMoveInRevolutions,
                                               int homeLimitSwitchPin)
{
  return (moveToHomeInSteps(directionTowardHome,
                            speedInRevolutionsPerSecond * stepsPerRevolution,
                            maxDistanceToMoveInRevolutions * stepsPerRevolution,
                            homeLimitSwitchPin));
}

//
// move relative to the current position, units are in revolutions, this function
// does not return until the move is complete
//  Enter:  distanceToMoveInRevolutions = signed distance to move relative to the
//          current position in revolutions
//
void ESP_FlexyStepper::moveRelativeInRevolutions(float distanceToMoveInRevolutions)
{
  setTargetPositionRelativeInRevolutions(distanceToMoveInRevolutions);

  while (!processMovement())
    ;
}

//
// setup a move relative to the current position, units are in revolutions, no
// motion occurs until processMove() is called
//  Enter:  distanceToMoveInRevolutions = signed distance to move relative to the
//            currentposition in revolutions
//
void ESP_FlexyStepper::setTargetPositionRelativeInRevolutions(
    float distanceToMoveInRevolutions)
{
  setTargetPositionRelativeInSteps((long)round(distanceToMoveInRevolutions *
                                               stepsPerRevolution));
}

//
// move to the given absolute position, units are in revolutions, this function
// does not return until the move is complete
//  Enter:  absolutePositionToMoveToInRevolutions = signed absolute position to
//            move to in units of revolutions
//
void ESP_FlexyStepper::moveToPositionInRevolutions(
    float absolutePositionToMoveToInRevolutions)
{
  setTargetPositionInRevolutions(absolutePositionToMoveToInRevolutions);

  while (!processMovement())
    ;
}

//
// setup a move, units are in revolutions, no motion occurs until processMove()
// is called
//  Enter:  absolutePositionToMoveToInRevolutions = signed absolute position to
//          move to in units of revolutions
//
void ESP_FlexyStepper::setTargetPositionInRevolutions(
    float absolutePositionToMoveToInRevolutions)
{
  setTargetPositionInSteps((long)round(absolutePositionToMoveToInRevolutions *
                                       stepsPerRevolution));
}

float ESP_FlexyStepper::getTargetPositionInRevolutions()
{
	return getTargetPositionInSteps() / stepsPerRevolution;
}

//
// Get the current velocity of the motor in revolutions/second.  This functions is
// updated while it accelerates up and down in speed.  This is not the desired
// speed, but the speed the motor should be moving at the time the function is
// called.  This is a signed value and is negative when the motor is moving
// backwards.  Note: This speed will be incorrect if the desired velocity is set
// faster than this library can generate steps, or if the load on the motor is too
// great for the amount of torque that it can generate.
//  Exit:  velocity speed in steps per second returned, signed
//
float ESP_FlexyStepper::getCurrentVelocityInRevolutionsPerSecond()
{
  return (getCurrentVelocityInStepsPerSecond() / stepsPerRevolution);
}

// ---------------------------------------------------------------------------------
//                        Public functions with units in steps
// ---------------------------------------------------------------------------------

//
// set the current position of the motor in steps, this does not move the motor
// Note: This function should only be called when the motor is stopped
//    Enter:  currentPositionInSteps = the new position of the motor in steps
//
void ESP_FlexyStepper::setCurrentPositionInSteps(long currentPositionInSteps)
{
  currentPosition_InSteps = currentPositionInSteps;
}

//
// get the current position of the motor in steps, this functions is updated
// while the motor moves
//  Exit:  a signed motor position in steps returned
//
long ESP_FlexyStepper::getCurrentPositionInSteps()
{
  return (currentPosition_InSteps);
}

//
// set the maximum speed, units in steps/second, this is the maximum speed reached
// while accelerating
//  Enter:  speedInStepsPerSecond = speed to accelerate up to, units in steps/second
//
void ESP_FlexyStepper::setSpeedInStepsPerSecond(float speedInStepsPerSecond)
{
  desiredSpeed_InStepsPerSecond = speedInStepsPerSecond;
  desiredPeriod_InUSPerStep = 1000000.0 / desiredSpeed_InStepsPerSecond;
}

//
// set the rate of acceleration, units in steps/second/second
//  Enter:  accelerationInStepsPerSecondPerSecond = rate of acceleration, units in
//          steps/second/second
//
void ESP_FlexyStepper::setAccelerationInStepsPerSecondPerSecond(
    float accelerationInStepsPerSecondPerSecond)
{
  acceleration_InStepsPerSecondPerSecond = accelerationInStepsPerSecondPerSecond;
  acceleration_InStepsPerUSPerUS = acceleration_InStepsPerSecondPerSecond / 1E12;

  periodOfSlowestStep_InUS =
      1000000.0 / sqrt(2.0 * acceleration_InStepsPerSecondPerSecond);
  minimumPeriodForAStoppedMotion = periodOfSlowestStep_InUS / 2.8;
}

//
// set the rate of deceleration, units in steps/second/second
//  Enter:  decelerationInStepsPerSecondPerSecond = rate of deceleration, units in
//          steps/second/second
//
void ESP_FlexyStepper::setDecelerationInStepsPerSecondPerSecond(
    float decelerationInStepsPerSecondPerSecond)
{
  deceleration_InStepsPerSecondPerSecond = decelerationInStepsPerSecondPerSecond;
  deceleration_InStepsPerUSPerUS = deceleration_InStepsPerSecondPerSecond / 1E12;
}

/**
 * set the current position as the home position (Step count = 0)
 */
void ESP_FlexyStepper::setCurrentPositionAsHomeAndStop()
{
  this->isOnWayToHome = false;
  this->currentStepPeriod_InUS = 0.0;
  this->nextStepPeriod_InUS = 0.0;
  this->directionOfMotion = 0;
  this->currentPosition_InSteps = 0;
  this->targetPosition_InSteps = 0;
  this->isCurrentlyHomed = true;
}

/**
 * start jogging in the direction of home (use setDirectionToHome() to set the proper direction) until the limit switch is hit, then set the position as home
 * Warning: This function requires a limit switch to be configured otherwise the motor will never stop jogging.
 * This is a non blocking function, you need make sure ESP_FlexyStepper is started as service (use startAsService() function) or need to call the processMovement function manually in your main loop.
 */
void ESP_FlexyStepper::goToLimitAndSetAsHome(callbackFunction callbackFunctionForHome)
{
  if (callbackFunctionForHome)
  {
    this->_homeReachedCallback = callbackFunctionForHome;
  }
  if (this->activeLimitSwitch == 0 || this->activeLimitSwitch != this->directionTowardsHome)
  {
    this->setTargetPositionInSteps(this->directionTowardsHome * 2000000000);
  }
  this->isOnWayToHome = true; //set as last action, since other functions might overwrite it
}

void ESP_FlexyStepper::goToLimit(signed char direction, callbackFunction callbackFunctionForLimit)
{
  if (callbackFunctionForLimit)
  {
    this->_callbackFunctionForGoToLimit = callbackFunctionForLimit;
  }

  if (this->activeLimitSwitch == 0)
  {
    this->setTargetPositionInSteps(direction * 2000000000);
  }
  this->isOnWayToLimit = true; //set as last action, since other functions might overwrite it
}

/**
 * register a callback function to be called whenever a movement to home has been completed (does not trigger when movement passes by the home position)
 */
void ESP_FlexyStepper::registerHomeReachedCallback(callbackFunction newFunction)
{
  this->_homeReachedCallback = newFunction;
}

/**
 * register a callback function to be called whenever a
 */
void ESP_FlexyStepper::registerLimitReachedCallback(callbackFunction limitSwitchTriggerdCallbackFunction)
{
  this->_limitTriggeredCallback = limitSwitchTriggerdCallbackFunction;
}

/**
 * register a callback function to be called whenever a target position has been reached
 */
void ESP_FlexyStepper::registerTargetPositionReachedCallback(positionCallbackFunction targetPositionReachedCallbackFunction)
{
  this->_targetPositionReachedCallback = targetPositionReachedCallbackFunction;
}

/**
 * register a callback function to be called whenever a emergency stop is triggered 
 */
void ESP_FlexyStepper::registerEmergencyStopTriggeredCallback(callbackFunction emergencyStopTriggerdCallbackFunction)
{
  this->_emergencyStopTriggeredCallback = emergencyStopTriggerdCallbackFunction;
}

/**
 * register a callback function to be called whenever the emergency stop switch is released
 */
void ESP_FlexyStepper::registerEmergencyStopReleasedCallback(callbackFunction emergencyStopReleasedCallbackFunction)
{
  this->_emergencyStopReleasedCallback = emergencyStopReleasedCallbackFunction;
}

/**
 * start jogging (continous movement without a fixed target position)
 * uses the currently set speed and acceleration settings
 * to stop the motion call the stopJogging function.
 * Will also stop when the external limit switch has been triggered using setLimitSwitchActive() or when the emergencyStop function is triggered
 * Warning: This function requires either a limit switch to be configured otherwise or manual trigger of the stopJogging/setTargetPositionToStop or emergencyStop function, the motor will never stop jogging
 */
void ESP_FlexyStepper::startJogging(signed char direction)
{
  this->setTargetPositionInSteps(direction * 2000000000);
}

/**
 * Stop jopgging, basically an alias function for setTargetPositionToStop()
 */
void ESP_FlexyStepper::stopJogging()
{
  this->setTargetPositionToStop();
}

//
// home the motor by moving until the homing sensor is activated, then set the
// position to zero with units in steps
//  Enter:  directionTowardHome = 1 to move in a positive direction, -1 to move in
//            a negative directions
//          speedInStepsPerSecond = speed to accelerate up to while moving toward
//            home, units in steps/second
//          maxDistanceToMoveInSteps = unsigned maximum distance to move toward
//            home before giving up
//          homeSwitchPin = pin number of the home switch, switch should be
//            configured to go low when at home
//  Exit:   true returned if successful, else false
//
bool ESP_FlexyStepper::moveToHomeInSteps(signed char directionTowardHome,
                                         float speedInStepsPerSecond, long maxDistanceToMoveInSteps,
                                         int homeLimitSwitchPin)
{
  float originalDesiredSpeed_InStepsPerSecond;
  bool limitSwitchFlag;

  //
  // setup the home switch input pin
  //
  pinMode(homeLimitSwitchPin, INPUT_PULLUP);

  //
  // remember the current speed setting
  //
  originalDesiredSpeed_InStepsPerSecond = desiredSpeed_InStepsPerSecond;

  //
  // if the home switch is not already set, move toward it
  //
  if (digitalRead(homeLimitSwitchPin) == HIGH)
  {
    //
    // move toward the home switch
    //
    setSpeedInStepsPerSecond(speedInStepsPerSecond);
    setTargetPositionRelativeInSteps(maxDistanceToMoveInSteps * directionTowardHome);
    limitSwitchFlag = false;
    while (!processMovement())
    {
      if (digitalRead(homeLimitSwitchPin) == LOW)
      {
        limitSwitchFlag = true;
        directionOfMotion = 0;
        break;
      }
    }

    //
    // check if switch never detected
    //
    if (limitSwitchFlag == false)
      return (false);

    if (this->_limitTriggeredCallback)
    {
      this->_limitTriggeredCallback();
    }
  }
  delay(25);

  //
  // the switch has been detected, now move away from the switch
  //
  setTargetPositionRelativeInSteps(maxDistanceToMoveInSteps *
                                   directionTowardHome * -1);
  limitSwitchFlag = false;
  while (!processMovement())
  {
    if (digitalRead(homeLimitSwitchPin) == HIGH)
    {
      limitSwitchFlag = true;
      directionOfMotion = 0;
      break;
    }
  }
  delay(25);

  //
  // check if switch never detected
  //
  if (limitSwitchFlag == false)
    return (false);

  //
  // have now moved off the switch, move toward it again but slower
  //
  setSpeedInStepsPerSecond(speedInStepsPerSecond / 8);
  setTargetPositionRelativeInSteps(maxDistanceToMoveInSteps * directionTowardHome);
  limitSwitchFlag = false;
  while (!processMovement())
  {
    if (digitalRead(homeLimitSwitchPin) == LOW)
    {
      limitSwitchFlag = true;
      directionOfMotion = 0;
      break;
    }
  }
  delay(25);

  //
  // check if switch never detected
  //
  if (limitSwitchFlag == false)
    return (false);

  //
  // successfully homed, set the current position to 0
  //
  setCurrentPositionInSteps(0L);

  setTargetPositionInSteps(0L);
  this->isCurrentlyHomed = true;
  this->disallowedDirection = directionTowardHome;
  //
  // restore original velocity
  //
  setSpeedInStepsPerSecond(originalDesiredSpeed_InStepsPerSecond);
  return (true);
}

//
// move relative to the current position, units are in steps, this function does
// not return until the move is complete
//  Enter:  distanceToMoveInSteps = signed distance to move relative to the current
//          position in steps
//
void ESP_FlexyStepper::moveRelativeInSteps(long distanceToMoveInSteps)
{
  setTargetPositionRelativeInSteps(distanceToMoveInSteps);

  while (!processMovement())
    ;
}

//
// setup a move relative to the current position, units are in steps, no motion
// occurs until processMove() is called
//  Enter:  distanceToMoveInSteps = signed distance to move relative to the current
//            positionin steps
//
void ESP_FlexyStepper::setTargetPositionRelativeInSteps(long distanceToMoveInSteps)
{
  setTargetPositionInSteps(currentPosition_InSteps + distanceToMoveInSteps);
}

//
// move to the given absolute position, units are in steps, this function does not
// return until the move is complete
//  Enter:  absolutePositionToMoveToInSteps = signed absolute position to move to
//            in unitsof steps
//
void ESP_FlexyStepper::moveToPositionInSteps(long absolutePositionToMoveToInSteps)
{
  setTargetPositionInSteps(absolutePositionToMoveToInSteps);

  while (!processMovement())
    ;
}

//
// setup a move, units are in steps, no motion occurs until processMove() is called
//  Enter:  absolutePositionToMoveToInSteps = signed absolute position to move to
//            in units of steps
//
void ESP_FlexyStepper::setTargetPositionInSteps(long absolutePositionToMoveToInSteps)
{
  //abort potentially running homing movement
  this->isOnWayToHome = false;
  this->isOnWayToLimit = false;
  targetPosition_InSteps = absolutePositionToMoveToInSteps;
  this->firstProcessingAfterTargetReached = true;
}

long ESP_FlexyStepper::getTargetPositionInSteps()
{
	return targetPosition_InSteps;
}

//
// setup a "Stop" to begin the process of decelerating from the current velocity
// to zero, decelerating requires calls to processMove() until the move is complete
// Note: This function can be used to stop a motion initiated in units of steps
// or revolutions
//
void ESP_FlexyStepper::setTargetPositionToStop()
{
  //abort potentially running homing movement
  this->isOnWayToHome = false;
  this->isOnWayToLimit = false;

  if(directionOfMotion == 0){
    return;
  }

  long decelerationDistance_InSteps;

  //
  // move the target position so that the motor will begin deceleration now
  //
  decelerationDistance_InSteps = (long)round(
      5E11 / (deceleration_InStepsPerSecondPerSecond * currentStepPeriod_InUS *
              currentStepPeriod_InUS));

  if (directionOfMotion > 0)
    setTargetPositionInSteps(currentPosition_InSteps + decelerationDistance_InSteps);
  else
    setTargetPositionInSteps(currentPosition_InSteps - decelerationDistance_InSteps);
}

//
// if it is time, move one step
//  Exit:  true returned if movement complete, false returned not a final target
//           position yet
//
bool ESP_FlexyStepper::processMovement(void)
{
  if (emergencyStopActive)
  {
    //abort potentially running homing movement
    this->isOnWayToHome = false;
    this->isOnWayToLimit = false;

    currentStepPeriod_InUS = 0.0;
    nextStepPeriod_InUS = 0.0;
    directionOfMotion = 0;
    targetPosition_InSteps = currentPosition_InSteps;

    //activate brake (if configured) driectly due to emergency stop if not already active
    if (this->_isBrakeConfigured && !this->_isBrakeActive)
    {
      this->activateBrake();
    }

    if (!this->holdEmergencyStopUntilExplicitRelease)
    {
      emergencyStopActive = false;
    }
    return (true);
  }

  //check if delayed brake shall be engaged / released
  if (this->_timeToEngangeBrake != LONG_MAX && this->_timeToEngangeBrake <= millis())
  {
    this->activateBrake();
  }
  else if (this->_timeToReleaseBrake != LONG_MAX && this->_timeToReleaseBrake <= millis())
  {
    this->deactivateBrake();
  }

  long distanceToTarget_Signed;

  //check if limit switch flag is active
  if (this->activeLimitSwitch != 0)
  {
    distanceToTarget_Signed = targetPosition_InSteps - currentPosition_InSteps;

    if (!this->limitSwitchCheckPeformed)
    {
      this->limitSwitchCheckPeformed = true;

      //a limit switch is active, so movement is only allowed in one direction (away from the switch)
      if (this->activeLimitSwitch == this->LIMIT_SWITCH_BEGIN)
      {
        this->disallowedDirection = this->directionTowardsHome;
      }
      else if (this->activeLimitSwitch == this->LIMIT_SWITCH_END)
      {
        this->disallowedDirection = this->directionTowardsHome * -1;
      }
      else if (this->activeLimitSwitch == this->LIMIT_SWITCH_COMBINED_BEGIN_AND_END)
      {
        //limit switches are paired together, so we need to try to figure out by checking which one it is, by using the last used step direction
        if (distanceToTarget_Signed > 0)
        {
          this->lastStepDirectionBeforeLimitSwitchTrigger = 1;
          this->disallowedDirection = 1;
        }
        else if (distanceToTarget_Signed < 0)
        {
          this->lastStepDirectionBeforeLimitSwitchTrigger = -1;
          this->disallowedDirection = -1;
        }
      }

      //movement has been triggerd by goToLimitAndSetAsHome() function. so once the limit switch has been triggered we have reached the limit and need to set it as home
      if (this->isOnWayToHome)
      {
        this->setCurrentPositionAsHomeAndStop(); //clear isOnWayToHome flag and stop motion

        if (this->_homeReachedCallback != NULL)
        {
          this->_homeReachedCallback();
        }
        //activate brake (or schedule activation) since we reached the final position
        if (this->_isBrakeConfigured && !this->_isBrakeActive)
        {
          this->triggerBrakeIfNeededOrSetTimeout();
        }
        return true;
      }
    }

    //check if further movement is allowed
    if (
        (this->disallowedDirection == 1 && distanceToTarget_Signed > 0) ||
        (this->disallowedDirection == -1 && distanceToTarget_Signed < 0))
    {
      //limit switch is acitve and movement in request direction is not allowed
      currentStepPeriod_InUS = 0.0;
      nextStepPeriod_InUS = 0.0;
      directionOfMotion = 0;
      targetPosition_InSteps = currentPosition_InSteps;
      //activate brake (or schedule activation) since limit is active for requested direction
      if (this->_isBrakeConfigured && !this->_isBrakeActive)
      {
        this->triggerBrakeIfNeededOrSetTimeout();
      }
      return true;
    }
  }

  unsigned long currentTime_InUS;
  unsigned long periodSinceLastStep_InUS;

  //
  // check if currently stopped
  //
  if (directionOfMotion == 0)
  {
    distanceToTarget_Signed = targetPosition_InSteps - currentPosition_InSteps;
    // check if target position in a positive direction
    if (distanceToTarget_Signed > 0)
    {
      directionOfMotion = 1;
      digitalWrite(directionPin, POSITIVE_DIRECTION);
      nextStepPeriod_InUS = periodOfSlowestStep_InUS;
      lastStepTime_InUS = micros();
      lastStepDirectionBeforeLimitSwitchTrigger = directionOfMotion;
      return (false);
    }

    // check if target position in a negative direction
    else if (distanceToTarget_Signed < 0)
    {
      directionOfMotion = -1;
      digitalWrite(directionPin, NEGATIVE_DIRECTION);
      nextStepPeriod_InUS = periodOfSlowestStep_InUS;
      lastStepTime_InUS = micros();
      lastStepDirectionBeforeLimitSwitchTrigger = directionOfMotion;
      return (false);
    }
    else
    {
      this->lastStepDirectionBeforeLimitSwitchTrigger = 0;
      //activate brake since motor is stopped
      if (this->_isBrakeConfigured && !this->_isBrakeActive && this->_hasMovementOccuredSinceLastBrakeRelease)
      {
        this->triggerBrakeIfNeededOrSetTimeout();
      }
      return (true);
    }
  }

  // determine how much time has elapsed since the last step (Note 1: this method
  // works even if the time has wrapped. Note 2: all variables must be unsigned)
  currentTime_InUS = micros();
  periodSinceLastStep_InUS = currentTime_InUS - lastStepTime_InUS;
  // if it is not time for the next step, return
  if (periodSinceLastStep_InUS < (unsigned long)nextStepPeriod_InUS)
    return (false);

  //we have to move, so deactivate brake (if configured at all) immediately
  if (this->_isBrakeConfigured && this->_isBrakeActive)
  {
    this->deactivateBrake();
  }

  // execute the step on the rising edge
  digitalWrite(stepPin, HIGH);

  // update the current position and speed
  currentPosition_InSteps += directionOfMotion;
  currentStepPeriod_InUS = nextStepPeriod_InUS;

  // remember the time that this step occured
  lastStepTime_InUS = currentTime_InUS;

  // figure out how long before the next step
  DeterminePeriodOfNextStep();

  // return the step line low
  digitalWrite(stepPin, LOW);

  this->_hasMovementOccuredSinceLastBrakeRelease = true;

  // check if the move has reached its final target position, return true if all
  // done
  if (currentPosition_InSteps == targetPosition_InSteps)
  {
    // at final position, make sure the motor is not going too fast
    if (nextStepPeriod_InUS >= minimumPeriodForAStoppedMotion)
    {
      currentStepPeriod_InUS = 0.0;
      nextStepPeriod_InUS = 0.0;
      directionOfMotion = 0;
      this->lastStepDirectionBeforeLimitSwitchTrigger = 0;

      if (this->firstProcessingAfterTargetReached)
      {
        firstProcessingAfterTargetReached = false;
        if (this->_targetPositionReachedCallback)
        {
          this->_targetPositionReachedCallback(currentPosition_InSteps);
        }
        //activate brake since we reached the final position
        if (this->_isBrakeConfigured && !this->_isBrakeActive)
        {
          this->triggerBrakeIfNeededOrSetTimeout();
        }
      }
      return (true);
    }
  }
  return (false);
}

/**
 * internal helper to determine if brake shall be acitvated (if configured at all) or if a delay needs to be set
 */
void ESP_FlexyStepper::triggerBrakeIfNeededOrSetTimeout()
{
  //check if break is already set or a timeout has already beend set
  if (this->_isBrakeConfigured && !this->_isBrakeActive && this->_timeToEngangeBrake == LONG_MAX)
  {
    if (this->_brakeReleaseDelayMs > 0 && this->_hasMovementOccuredSinceLastBrakeRelease)
    {
      this->_timeToReleaseBrake = millis() + this->_brakeReleaseDelayMs;
    }

    if (this->_brakeEngageDelayMs == 0)
    {
      this->activateBrake();
    }
    else
    {
      this->_timeToEngangeBrake = millis() + this->_brakeEngageDelayMs;
    }
  }
}

// Get the current velocity of the motor in steps/second.  This functions is
// updated while it accelerates up and down in speed.  This is not the desired
// speed, but the speed the motor should be moving at the time the function is
// called.  This is a signed value and is negative when the motor is moving
// backwards.  Note: This speed will be incorrect if the desired velocity is set
// faster than this library can generate steps, or if the load on the motor is too
// great for the amount of torque that it can generate.
//  Exit:  velocity speed in steps per second returned, signed
//
float ESP_FlexyStepper::getCurrentVelocityInStepsPerSecond()
{
  if (currentStepPeriod_InUS == 0.0)
    return (0);
  else
  {
    if (directionOfMotion > 0)
      return (1000000.0 / currentStepPeriod_InUS);
    else
      return (-1000000.0 / currentStepPeriod_InUS);
  }
}

//
// check if the motor has competed its move to the target position
//  Exit:  true returned if the stepper is at the target position
//
bool ESP_FlexyStepper::motionComplete()
{
  if ((directionOfMotion == 0) &&
      (currentPosition_InSteps == targetPosition_InSteps))
    return (true);
  else
    return (false);
}

//
// determine the period for the next step, either speed up a little, slow down a
// little or go the same speed
//
void ESP_FlexyStepper::DeterminePeriodOfNextStep()
{
  long distanceToTarget_Signed;
  long distanceToTarget_Unsigned;
  long decelerationDistance_InSteps;
  float currentStepPeriodSquared;
  bool speedUpFlag = false;
  bool slowDownFlag = false;
  bool targetInPositiveDirectionFlag = false;
  bool targetInNegativeDirectionFlag = false;

  //
  // determine the distance to the target position
  //
  distanceToTarget_Signed = targetPosition_InSteps - currentPosition_InSteps;
  if (distanceToTarget_Signed >= 0L)
  {
    distanceToTarget_Unsigned = distanceToTarget_Signed;
    targetInPositiveDirectionFlag = true;
  }
  else
  {
    distanceToTarget_Unsigned = -distanceToTarget_Signed;
    targetInNegativeDirectionFlag = true;
  }

  //
  // determine the number of steps needed to go from the current speed down to a
  // velocity of 0, Steps = Velocity^2 / (2 * Deceleration)
  //
  currentStepPeriodSquared = currentStepPeriod_InUS * currentStepPeriod_InUS;
  decelerationDistance_InSteps = (long)round(
      5E11 / (deceleration_InStepsPerSecondPerSecond * currentStepPeriodSquared));

  //
  // check if: Moving in a positive direction & Moving toward the target
  //    (directionOfMotion == 1) && (distanceToTarget_Signed > 0)
  //
  if ((directionOfMotion == 1) && (targetInPositiveDirectionFlag))
  {
    //
    // check if need to start slowing down as we reach the target, or if we
    // need to slow down because we are going too fast
    //
    if ((distanceToTarget_Unsigned < decelerationDistance_InSteps) ||
        (nextStepPeriod_InUS < desiredPeriod_InUSPerStep))
      slowDownFlag = true;
    else
      speedUpFlag = true;
  }

  //
  // check if: Moving in a positive direction & Moving away from the target
  //    (directionOfMotion == 1) && (distanceToTarget_Signed < 0)
  //
  else if ((directionOfMotion == 1) && (targetInNegativeDirectionFlag))
  {
    //
    // need to slow down, then reverse direction
    //
    if (currentStepPeriod_InUS < periodOfSlowestStep_InUS)
    {
      slowDownFlag = true;
    }
    else
    {
      directionOfMotion = -1;
      digitalWrite(directionPin, NEGATIVE_DIRECTION);
    }
  }

  //
  // check if: Moving in a negative direction & Moving toward the target
  //    (directionOfMotion == -1) && (distanceToTarget_Signed < 0)
  //
  else if ((directionOfMotion == -1) && (targetInNegativeDirectionFlag))
  {
    //
    // check if need to start slowing down as we reach the target, or if we
    // need to slow down because we are going too fast
    //
    if ((distanceToTarget_Unsigned < decelerationDistance_InSteps) ||
        (nextStepPeriod_InUS < desiredPeriod_InUSPerStep))
      slowDownFlag = true;
    else
      speedUpFlag = true;
  }

  //
  // check if: Moving in a negative direction & Moving away from the target
  //    (directionOfMotion == -1) && (distanceToTarget_Signed > 0)
  //
  else if ((directionOfMotion == -1) && (targetInPositiveDirectionFlag))
  {
    //
    // need to slow down, then reverse direction
    //
    if (currentStepPeriod_InUS < periodOfSlowestStep_InUS)
    {
      slowDownFlag = true;
    }
    else
    {
      directionOfMotion = 1;
      digitalWrite(directionPin, POSITIVE_DIRECTION);
    }
  }

  //
  // check if accelerating
  //
  if (speedUpFlag)
  {
    //
    // StepPeriod = StepPeriod(1 - a * StepPeriod^2)
    //
    nextStepPeriod_InUS = currentStepPeriod_InUS - acceleration_InStepsPerUSPerUS *
                                                       currentStepPeriodSquared * currentStepPeriod_InUS;

    if (nextStepPeriod_InUS < desiredPeriod_InUSPerStep)
      nextStepPeriod_InUS = desiredPeriod_InUSPerStep;
  }

  //
  // check if decelerating
  //
  if (slowDownFlag)
  {
    //
    // StepPeriod = StepPeriod(1 + a * StepPeriod^2)
    //
    nextStepPeriod_InUS = currentStepPeriod_InUS + deceleration_InStepsPerUSPerUS *
                                                       currentStepPeriodSquared * currentStepPeriod_InUS;

    if (nextStepPeriod_InUS > periodOfSlowestStep_InUS)
      nextStepPeriod_InUS = periodOfSlowestStep_InUS;
  }
}

// -------------------------------------- End --------------------------------------
