#include "FastAccelStepper.h"

// for avr: either use pin 9 or 10 aka OC1A or OC1B
#define stepPinStepper 17
#define enablePinStepper 26
#define dirPinStepper 18

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper;

void setup() {
  Serial.begin(115200);
  Serial.println("Demo RawAccessWithDelay");

  engine.init();

  stepper = engine.stepperConnectToPin(stepPinStepper);

  if (stepper) {
    stepper->setDirectionPin(dirPinStepper);
    stepper->setEnablePin(enablePinStepper);
    stepper->setAutoEnable(false);
    stepper->enableOutputs();
  }
}

// This loop should use the new pause only command to generate steps every 0.1 s

void loop() {
  // in case the stepper has not gotten initialized
  if (!stepper) {
    return;
  }
  // every loop will create one step and then pause 0.1 s = 25 * 4ms
  const struct stepper_command_s cmd_step = {
      .ticks = 64000, .steps = 1, .count_up = true};  // one step with pause 4ms
  const struct stepper_command_s cmd_pause = {
      .ticks = 64000, .steps = 0, .count_up = true};  // only pause 4ms

  // create one step + 4 ms pause
  while (true) {
    // This loop repeats a command, in case the queue is full
    int rc = stepper->addQueueEntry(&cmd_step);
    // Serial.println(rc);
    if (rc == AQE_OK) {
      Serial.println("STEP");
      break;
    }
    // adding a delay(1) causes problems
    delayMicroseconds(1000);
  }

  // this creates 24*4ms pauses
  for (uint16_t i = 0; i < 24; i++) {
    // This loop repeats a command, in case the queue is full
    while (true) {
      int rc = stepper->addQueueEntry(&cmd_pause);
      // Serial.println(rc);
      if (rc == AQE_OK) {
        Serial.print(".");
        break;
      }
      // adding a delay(1) causes problems
      delayMicroseconds(1000);
    }
  }
}
