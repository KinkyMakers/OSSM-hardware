
//+++++++++++++++++++++++++++++++++++
// OSSM Basic hardware PULSE test
// Use this to make sure your tools 
//+++++++++++++++++++++++++++++++++++

// Initial: James C. July 2021
// Last updated by: James C. July 2021

// =============================================================
// Start here by setting the pins to the ones you expect to use
// These are filled out with the standard pins used by the wifi code
//
//  Most servos are enabled LOW, it's recommended in that case to try without
//  connecting the enable pin, however if it is connected we've forced it low
//  If your servo requires a HIGH signal to be enabled, switch it below 
//
// =============================================================

const int pulPin = 27; // Make this the same as your PULSE pin
const int dirPin = 25; // Make this the same as your DIRECTION pin
const int enablePin = 22; // Make this the same as your ENABLE pin


void setup() {

 // setups the pins based on your 
  
 pinMode(pulPin, OUTPUT);
 pinMode(dirPin, OUTPUT);
 pinMode(enablePin, OUTPUT);

 
 Serial.begin(9600);
 Serial.println("I'm awake");


// ========
// Change this if your server is enable HIGH
// =========

 digitalWrite(enablePin, LOW);
 
}

void loop() {
  // put your main code here, to run repeatedly:
 Serial.println("I'm starting one direction");
 digitalWrite(dirPin, HIGH);
 for( int i = 0; i < 1000; i++){
  digitalWrite(pulPin, HIGH);
  delayMicroseconds(50);
  digitalWrite(pulPin, LOW);
  delayMicroseconds(50);
 }
 Serial.println("Now the other direction");
  digitalWrite(dirPin, LOW);
 for( int i = 0; i < 1000; i++){
  digitalWrite(pulPin, HIGH);
  delayMicroseconds(50);
  digitalWrite(pulPin, LOW);
  delayMicroseconds(50);
 }
}
