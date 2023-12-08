
//      ******************************************************************
//      *                                                                *
//      *         Simple example for controlling a stepper motor         *
//      *                                                                *
//      *            Paul Kerspe               4.6.2020                  *
//      *                                                                *
//      ******************************************************************

// This is the simplest example of how to run a stepper motor.  
//
// Documentation for this library can be found at:
//    https://github.com/pkerspe/ESP-FlexyStepper/blob/master/README.md
//
// This library requires that your stepper motor be connected to the ESP 
// using a driver that has a "Step and Direction" interface.  
// Examples of these are:
//
//    Pololu's DRV8825 Stepper Motor Driver Carrier:
//        https://www.pololu.com/product/2133
//
//    Pololu's A4988 Stepper Motor Driver Carrier:
//        https://www.pololu.com/product/2980
//
//    Sparkfun's Big Easy Driver:
//        https://www.sparkfun.com/products/12859
//
//    GeckoDrive G203V industrial controller:
//        https://www.geckodrive.com/g203v.html
//
// For all driver boards, it is VERY important that you set the motor 
// current before running the example.  This is typically done by adjusting
// a potentiometer on the board.  Read the driver board's documentation to 
// learn how.

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
  //
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
  // Note 3: This example uses "relative" motions.  This means that each
  // command will move the number of steps given, starting from it's 
  // current position.
  //

  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(100);
  stepper.setAccelerationInStepsPerSecondPerSecond(100);

  // Rotate the motor in the forward direction one revolution (200 steps). 
  // This function call will not return until the motion is complete.
  stepper.moveRelativeInSteps(200);
  delay(1000);
  // rotate backward 1 rotation, then wait 1 second
  stepper.moveRelativeInSteps(-200);
  delay(1000);

  // This time speedup the motor, turning 10 revolutions.  Note if you
  // tell a stepper motor to go faster than it can, it just stops.
  stepper.setSpeedInStepsPerSecond(800);
  stepper.setAccelerationInStepsPerSecondPerSecond(800);
  stepper.moveRelativeInSteps(200 * 10);
  delay(2000);
}
