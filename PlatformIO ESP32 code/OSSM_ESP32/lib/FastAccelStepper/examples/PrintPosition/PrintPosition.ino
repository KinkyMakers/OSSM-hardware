#include "FastAccelStepper.h"

// As in StepperDemo for Motor 1 on AVR
#define dirPinStepper 5
#define enablePinStepper 25
#define stepPinStepper 7

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper = NULL;

void setup() {
  Serial.begin(115200);

  Serial.println("Starting");
  Serial.flush();
  engine.init();
  stepper = engine.stepperConnectToPin(stepPinStepper);
  Serial.println("Starting");
  Serial.print("Stepper Pin:");
  Serial.println(stepPinStepper);
  Serial.flush();
  Serial.println((unsigned int)stepper);
  Serial.println((unsigned int)&engine);
  if (stepper) {
    stepper->setDirectionPin(dirPinStepper);
    stepper->setEnablePin(enablePinStepper);
    stepper->setAutoEnable(true);

    stepper->setSpeedInUs(1000);  // the parameter is us/step !!!
    stepper->setAcceleration(10000);
    // stepper->move(7000);
    stepper->runForward();
  } else {
    Serial.println("Stepper Not initialized!");
    delay(1000);
  }
  Serial.print("    F_CPU=");
  Serial.println(F_CPU);
  Serial.print("    TICKS_PER_S=");
  Serial.println(TICKS_PER_S);
  Serial.flush();
}

void loop() {
  delay(100);
  if (stepper) {
    if (stepper->isRunning()) {
      Serial.print("@");
      Serial.println(stepper->getCurrentPosition());
    }
  } else {
    Serial.println("Stepper died?");
    Serial.flush();
    delay(10000);
  }
}
