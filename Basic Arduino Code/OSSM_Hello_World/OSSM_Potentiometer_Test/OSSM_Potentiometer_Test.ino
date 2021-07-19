// Written by Alexica 
//
// Simple potentiometer test for help while comissioning


// Potentiometer is connected to GPIO 34 (Analog ADC1_CH6) 
const int potPin = 34; // REPLACE PIN WITH WHATEVER PIN THE POT YOU ARE TRYING TO TEST IS ON
 
// variable for storing the potentiometer value
int potValue = 0;
 
void setup() {
  Serial.begin(115200);
  delay(1000);
}
 
void loop() {
  // Reading potentiometer value
  potValue = analogRead(potPin);
  Serial.println(potValue);
  delay(250);
}
