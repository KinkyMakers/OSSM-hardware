
//      ******************************************************************
//      *                                                                *
//      *       Using an emergency switch to stop all motion instantly   *
//      *                                                                *
//      *            Paul Kerspe               4.6.2020                  *
//      *                                                                *
//      ******************************************************************

// This example shows how to use an emergency switch (a.k.a. kill switch) with an iterrupt to stop ongoing motion instantly
//
// In order to run the example, change the IO pin numbers to match your configuration (lines 21-24)
// you might also want to change the speed and distance settings according to your setup if needed (lines 27-30)
//
// Documentation for this library can be found at:
//    https://github.com/pkerspe/ESP-FlexyStepper/blob/master/README.md
//

#include <ESP_FlexyStepper.h>

// IO pin assignments
const int MOTOR_STEP_PIN = 33;
const int MOTOR_DIRECTION_PIN = 25;
const int EMERGENCY_STOP_PIN = 13; //define the IO PIN the emergency stop switch is connected to

// Speed settings
const int DISTANCE_TO_TRAVEL_IN_STEPS = 500;
const int SPEED_IN_STEPS_PER_SECOND = 800;
const int ACCELERATION_IN_STEPS_PER_SECOND = 500;
const int DECELERATION_IN_STEPS_PER_SECOND = 500;

// create the stepper motor object
ESP_FlexyStepper stepper;
int previousDirection = 1;
bool emergencySwitchTriggered = 0;

/**
 * the iterrupt service routine (ISR) for the emergency swtich
 * this gets called on a rising edge on the IO Pin the emergency switch is connected
 * it only sets the emergencySwitchTriggered flag and then returns. 
 * The actual emergency stop will than be handled in the loop function
 */
void ICACHE_RAM_ATTR emergencySwitchHandler(){
  emergencySwitchTriggered = 1;
}

void setup() 
{
  Serial.begin(115200);
  //set the pin for the emegrenxy witch to input with inernal pullup
  //the emergency switch is connected in a Active Low configuraiton in this example, meaning the switch connects the input to ground when closed
  pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);
  //attach an interrupt to the IO pin of the switch and specify the handler function 
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_STOP_PIN), emergencySwitchHandler, RISING);
  // connect and configure the stepper motor to its IO pins
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepper.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);
}

void loop() 
{
  if(stepper.getDirectionOfMotion() == 0){
    delay(4000);
    previousDirection *= -1;
    stepper.setTargetPositionRelativeInSteps(DISTANCE_TO_TRAVEL_IN_STEPS * previousDirection);
  }
  if(emergencySwitchTriggered){
    //clear the target position (if it the stepper is moving at all) and stop moving
    stepper.emergencyStop();
    //clear the flag that has been set in the ISR 
    emergencySwitchTriggered = 0;
  }
  //call the function to update the position of the stepper in a non blocking way
  //for smooth movements this requires that the loop function in general has no blocking call to other functions and can be processes in a rather short time
  //if you need to perform long running operations in the loop, this approach will not work for you and you should start a vtask instead that calls the processMovment function
  stepper.processMovement();
}
