//      ***********************************************************************
//      *                            Example  4:                              *
//      * Using a combined (switch at begin and end of track wired in serial) *
//      *            limit switch setup + latching emergency stop switch      *
//      *                                                                     *
//      *               Paul Kerspe               8.6.2020                    *
//      *                                                                     *
//      ***********************************************************************

// This example shows how to use a latching emergency switch (a.k.a. kill switch) in a normaly open configuration with an iterrupt to stop ongoing motion instantly
// as well as the use of two limit switches that are wired in serial to use up only a single IO pin of the ESP.
// all switches are connected to ground with one pin and the other pin connected to the configured IO pin.
// the limit switches are configured in a normaly closed configuration and in serial (output of the first switch connected to the input of the second switch)
//
// NOTE: this example relies on a rather quick processing time of the loop() function. If you are putting too much code in there (and by that increasing the processing time of each loop run) you will delay the trigger of the limit switch.
// if your limit switch is too close to the physical end of your motion track and the stepper is running at high speed, you are running the risk to shoot over the limit switch and into the physical limits of your track.
//
// In order to run the example, change the IO pin numbers to match your configuration (lines 29-32)
// you might also want to change the speed and distance settings according to your setup if needed (lines 35-38)
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

//variables for software debouncing of the limit switches
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 100; //the minimum delay in milliseconds to check for bouncing of the switch. Increase this slighlty if you switches tend to bounce a lot
bool buttonStateChangeDetected = false;
byte limitSwitchState = 0;
byte oldConfirmedLimitSwitchState = 0;

/**
 * the iterrupt service routine (ISR) for the emergency swtich
 * this gets called on a rising edge on the IO Pin the emergency switch is connected
 * it only sets the emergencySwitchTriggered flag and then returns. 
 * The actual emergency stop will than be handled in the loop function
 */
void ICACHE_RAM_ATTR emergencySwitchHandler()
{
  // we do not realy need to debounce here, since we only want to trigger a stop, no matter what.
  // So even triggering mutliple times does not realy matter at the end
  if (digitalRead(EMERGENCY_STOP_PIN) == LOW) // Switch is configured in active low configuration
  {
    // the boolean true in the following command tells the stepper to hold the emergency stop until reaseEmergencyStop() is called explicitly.
    // If ommitted or "false" is given, the function call would only stop the current motion and then instanlty would allow for new motion commands to be accepted
    stepper.emergencyStop(true);
  }
  else
  {
    // release a previously enganed emergency stop when the emergency stop button is released
    stepper.releaseEmergencyStop();
  }
}

void limitSwitchHandler()
{
  limitSwitchState = digitalRead(LIMIT_SWITCH_PIN);
  lastDebounceTime = millis();
}

void setup()
{
  Serial.begin(115200);
  //set the pin for the emegrendy switch and limit switch to input with inernal pullup
  //the emergency switch is connected in a Active Low configuraiton in this example, meaning the switch connects the input to ground when closed
  pinMode(EMERGENCY_STOP_PIN, INPUT_PULLUP);
  pinMode(LIMIT_SWITCH_PIN, INPUT_PULLUP);

  //attach an interrupt to the IO pin of the ermegency stop switch and specify the handler function
  attachInterrupt(digitalPinToInterrupt(EMERGENCY_STOP_PIN), emergencySwitchHandler, RISING);

  //attach an interrupt to the IO pin of the limit switch and specify the handler function
  attachInterrupt(digitalPinToInterrupt(LIMIT_SWITCH_PIN), limitSwitchHandler, CHANGE);

  // tell the ESP_flexystepper in which direction to move when homing is required (only works with a homing / limit switch connected)
  stepper.setDirectionToHome(-1);

  // connect and configure the stepper motor to its IO pins
  stepper.connectToPins(MOTOR_STEP_PIN, MOTOR_DIRECTION_PIN);

  // set the speed and acceleration rates for the stepper motor
  stepper.setSpeedInStepsPerSecond(SPEED_IN_STEPS_PER_SECOND);
  stepper.setAccelerationInStepsPerSecondPerSecond(ACCELERATION_IN_STEPS_PER_SECOND);
  stepper.setDecelerationInStepsPerSecondPerSecond(DECELERATION_IN_STEPS_PER_SECOND);
}

void loop()
{
  if (limitSwitchState != oldConfirmedLimitSwitchState && (millis() - lastDebounceTime) > debounceDelay)
  {
    oldConfirmedLimitSwitchState = limitSwitchState;
    Serial.printf("Limit switch change detected. New state is %i\n", limitSwitchState);
    //active high switch configuration (NC connection with internal pull up)
    if (limitSwitchState == HIGH)
    {
      stepper.setLimitSwitchActive(stepper.LIMIT_SWITCH_COMBINED_BEGIN_AND_END); // this will cause to stop any motion that is currently going on and block further movement in the same direction as long as the switch is agtive
    }
    else
    {
      stepper.clearLimitSwitchActive(); // clear the limit switch flag to allow movement in both directions again
    }
  }

  // just move the stepper back and forth in an endless loop
  if (stepper.getDirectionOfMotion() == 0)
  {
    previousDirection *= -1;
    long targetPosition = DISTANCE_TO_TRAVEL_IN_STEPS * previousDirection;
    stepper.setTargetPositionRelativeInSteps(targetPosition);
  }

  // call the function to update the position of the stepper in one increment
  // for smooth movements this requires that the loop function in general has no long lasting blocking call (or delays) to other functions and can be processes in a rather short time
  // if you need to perform long running operations in the loop, this approach will not work for you and you should start a vtask instead that calls the processMovment function
  // see example 5 for an example using a separate task on an ESP32 module to keep the loop completely free for your own code without bothering with slim code or blocking calls
  stepper.processMovement();
}
