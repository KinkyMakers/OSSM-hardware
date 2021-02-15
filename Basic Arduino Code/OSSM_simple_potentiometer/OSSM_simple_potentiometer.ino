//
//
//Based on accellStepper demo - Make a single stepper bounce from one limit to another
//
//Modified to work with OSSM sex machine
//
//This is a very simple method of control that only updates the speed and stroke after each move completes
//This means it responds to potentiometer changes at each end of stroke.
//
//

#include <AccelStepper.h>

#define enable 12
const int analogSpeedPin = A0;//connect your potentiometer for speed here
const int analogStrokePin = A1;//connect your potentiometer for stroke length here

//you will need to change these parameters based on your stroke length, stepper capability, and step resolution
//if stepper is skipping while moving, lower max speed
//if stroke is too short, increase strokeLimit
//if speed and stroke are low, increase both (likely variance in microsteps) These settings were used with 200 steps/rev, but with cheap driver this will be noisy
const int speedLimit = 1000;
const int speedMin = 100;
const int strokeLimit = 15000;
const int strokeMin = 100;

float analogSpeed = 0;
float speedValue = 0;

float analogStroke = 0;
float strokeValue = 0;


// Define a stepper and the pins it will use
AccelStepper stepper(1,2,3); // Defaults to AccelStepper::FULL4WIRE (4 pins) on 2, 3, 4, 5

void setup()
{  
    Serial.begin(9600);
  // Change these to suit your stepper if you want
    analogSpeed = analogRead(analogSpeedPin);
    speedValue = map(analogSpeed, 0, 1023, speedMin, speedLimit);
    analogStroke = analogRead(analogStrokePin);
    strokeValue = map(analogStroke, 0, 1023, strokeMin, strokeLimit);
    
    stepper.setMaxSpeed(speedValue);
    stepper.setAcceleration(3.0*speedValue);
    stepper.moveTo(strokeValue);
    
}

void loop()
{
    // If at the end of travel go to the other end

    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
    stepper.run();
        
    if (stepper.distanceToGo() == 0){
      while(analogRead(analogSpeedPin) <80){
        delay(10);
        //this allows you to stop the machine by setting the speed pot low.
        //without this, it might start a move at a very slow speed that would not respond until the stroke was complete
      }
    analogSpeed = analogRead(analogSpeedPin);
    speedValue = map(analogSpeed, 0, 1023, speedMin, speedLimit);
    analogStroke = analogRead(analogStrokePin);
    strokeValue = map(analogStroke, 0, 1023, strokeMin, strokeLimit);
      
    stepper.setMaxSpeed(speedValue);
    stepper.setAcceleration(speedValue*6);
    //stepper.moveTo(strokeValue);
      stepper.moveTo(0);
    }


      
    if (stepper.distanceToGo() == 0){
      while(analogRead(analogSpeedPin) <80){
        delay(10);
      }
    analogSpeed = analogRead(analogSpeedPin);
    speedValue = map(analogSpeed, 0, 1023, speedMin, speedLimit);
    analogStroke = analogRead(analogStrokePin);
    strokeValue = map(analogStroke, 0, 1023, strokeMin, strokeLimit);
    
    stepper.setMaxSpeed(speedValue);
    stepper.setAcceleration(speedValue*6);
      stepper.moveTo(strokeValue);
      Serial.print("speed = ");
      Serial.print(analogSpeed);
      Serial.print("\t stroke = ");
      Serial.println(analogStroke);

    }


}
