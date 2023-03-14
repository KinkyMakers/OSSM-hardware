
//      ******************************************************************
//      *                                                                *
//      *       Using "absolute" moves to position a stepper motor       *
//      *                                                                *
//      *            Paul Kerspe               4.6.2020                  *
//      *                                                                *
//      ******************************************************************

// This example is similar to Example 1 except that is uses "absolute" 
// moves instead of "relative" ones.  Relative moves will use a coordinate 
// system that is relative to the motor's current position.  Absolute moves 
// use a coordinate system that is referenced to the original position of 
// the motor when it is turned on.
//
// For example moving relative 200 steps, then another 200, then another  
// 200 will turn 600 steps in total (and end up at an absolute position of 
// 600).
//
// However issuing an absolute move to position 200 will rotate forward one   
// rotation.  Then running the next absolute move to position 400 will turn  
// just one more revolution in the same direction.  Finally moving to position 0  
// will rotate backward two revolutions, back to the starting position.
//  
//
// Documentation for this library can be found at:
//    https://github.com/pkerspe/ESP-FlexyStepper/blob/master/README.md
//

#include <ESP_FlexyStepper.h>

// IO pin assignments
const int MOTOR_STEP_PIN = 3;
const int MOTOR_DIRECTION_PIN = 4;

// create the stepper motor object
ESP_FlexyStepper stepper;

void setup() 
{
  Serial.begin(115200);
  // connect and configure the stepper motor to its IO pins
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);
}

void loop() 
{
  // Note 1: It is assumed that you are using a stepper motor with a 
  // 1.8 degree step angle (which is 200 steps/revolution). This is the
  // most common type of stepper.
  //
  // Note 2: It is also assumed that your stepper driver board is  
  // configured for 1x microstepping.
  //
  // It is OK if these assumptions are not correct, your motor will just
  // turn less than a full rotation when commanded to. 
  //
  // Note 3: This example uses "absolute" motions, meaning the values
  // sent to the move commands use a coordinate system where 0 is the
  // initial position of the motor when it is first turned on.

  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(100);
  stepper.setAccelerationInStepsPerSecondPerSecond(100);
  // Rotate the motor to position 200 (turning one revolution). This 
  // function call will not return until the motion is complete.
  stepper.moveToPositionInSteps(200);
  delay(1000);
  // rotate to position 400 (turning just one revolution more since it is 
  // already at position 200)
  stepper.moveToPositionInSteps(400);
  delay(1000);
  // Now speedup the motor and return to it's home position of 0, resulting 
  // in rotating backward two turns.  Note if you tell a stepper motor to go 
  // faster than it can, it just stops.
  stepper.setSpeedInStepsPerSecond(250);
  stepper.setAccelerationInStepsPerSecondPerSecond(800);
  stepper.moveToPositionInSteps(0);
  delay(2000);
}
