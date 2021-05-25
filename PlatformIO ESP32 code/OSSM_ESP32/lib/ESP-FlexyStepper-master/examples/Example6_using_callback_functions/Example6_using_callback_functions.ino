//      ***********************************************************************
//      *                            Example  6:                              *
//      * like Example 5 this example shows how to use the ESP FlexyStepper   *
//      * as a service running in the background, to keep the loop() free for *
//      * your own code, addtionally this function shows how to use callback  *
//      * functions to react to event like when the stepper reaches its       *
//      * target positon
//      *                                                                     *
//      *               Paul Kerspe               4.8.2020                    *
//      *                                                                     *
//      ***********************************************************************

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

//this function gets called whenever the stepper reaches the target position / ends the current movement
//it is registerd in the line "stepperConfiguration->getFlexyStepper()->registerTargetPositionReachedCallback(targetPositionReachedCallback);"
void targetPositionReachedCallback(long position)
{
  Serial.printf("Stepper reached target position %ld\n", position);

  previousDirection *= -1;
  long relativeTargetPosition = DISTANCE_TO_TRAVEL_IN_STEPS * previousDirection;
  Serial.printf("Moving stepper by %ld steps\n", relativeTargetPosition);
  stepper.setTargetPositionRelativeInSteps(relativeTargetPosition);
}

void setup()
{
  Serial.begin(115200);
  // connect and configure the stepper motor to its IO pins
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);
  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepper.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);

  //register the callback function, to get informed whenever the target postion has been reached
  stepper.registerTargetPositionReachedCallback(targetPositionReachedCallback);
  //you can also register for other events using (these accept currently only functions without any parameters):
  // stepper.registerEmergencyStopReleasedCallback(...);
  // stepper.registerEmergencyStopTriggeredCallback(...);
  // stepper.registerHomeReachedCallback(...);
  // stepper.registerLimitReachedCallback(...);

  // Not start the stepper instance as a service in the "background" as a separate task
  // and the OS of the ESP will take care of invoking the processMovement() task regularily so you can do whatever you want in the loop function
  stepper.startAsService();

  //send the stepper on its journey. Once the intial position has been reached, the callback function will get triggered and send the stepper to a new destination
  stepper.setTargetPositionRelativeInSteps(DISTANCE_TO_TRAVEL_IN_STEPS * previousDirection);
}

void loop()
{
  // Notice that you can now do whatever you want in the loop function without the need to call processMovement().
  // also you do not have to care if your loop processing times are too long.
}
