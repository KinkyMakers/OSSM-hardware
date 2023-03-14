#include <avr/sleep.h>
#include <AVRStepperPins.h>
#include <RampCalculator.h>

float acceleration_f;
uint32_t acceleration_i;
uint32_t steps;

void setup() {
  Serial.begin(115200);
  Serial.println("Start");

  digitalWrite(stepPinStepperA, LOW);
  digitalWrite(stepPinStepperB, LOW);
  pinMode(stepPinStepperA, OUTPUT);
  pinMode(stepPinStepperB, OUTPUT);

  acceleration_f = 12345.0;
  acceleration_i = 12345.0;
  steps = 10;
}
void loop() {
  digitalWrite(stepPinStepperA, HIGH);
  uint32_t x;
  // x = calculate_ticks_v1(steps, acceleration_f);
  // x = calculate_ticks_v2(steps, acceleration_f);
  // x = calculate_ticks_v3(steps, acceleration_f);
  // x = calculate_ticks_v4(steps, acceleration_i);
  // x = calculate_ticks_v5(steps, acceleration_i);
  // x = calculate_ticks_v7(0x1234000, 1000);
  x = calculate_ticks_v8(0x1234000, 1000);
  digitalWrite(stepPinStepperA, LOW);

  digitalWrite(stepPinStepperB, HIGH);
  digitalWrite(stepPinStepperB, LOW);
  Serial.println(x);
  delay(100);
  noInterrupts();
  sleep_cpu();
}
