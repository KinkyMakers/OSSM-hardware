// Written by Alexica 
//
// Simple potentiometer test for help while comissioning


// Potentiometer Connections for the normal ESP32 OSSM Setup.
const int StrokePotPin = 32;
const int SpeedPotPin = 33; 

// variables for storing the potentiometer value
int StrokeValue = 0;
int SpeedValue = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
}

void loop() {
  // Reading potentiometer values
  StrokeValue = analogRead(StrokePotPin);
  SpeedValue = analogRead(SpeedPotPin);
  
  Serial.print("Stroke Value ="); //Printing Pot values w/ labels.
  Serial.println(StrokeValue);
  delay(250);
  Serial.print("Speed Value =");
  Serial.println(SpeedValue);
  delay(250);
}
