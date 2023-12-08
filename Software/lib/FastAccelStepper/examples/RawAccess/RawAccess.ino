#include "FastAccelStepper.h"

// for avr: either use pin 9 or 10 aka OC1A or OC1B
#define stepPinStepper 17
#define enablePinStepper 26
#define dirPinStepper 18

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper;

void setup() {
  Serial.begin(115200);
  Serial.println("Demo RawAccess");

  engine.init();

  stepper = engine.stepperConnectToPin(stepPinStepper);

  if (stepper) {
    stepper->setDirectionPin(dirPinStepper);
    stepper->setEnablePin(enablePinStepper);
    stepper->setAutoEnable(true);
  }
}

// This loop drives the stepper up to 50000 microsteps/s.
// For a NEMA-17 with 200 steps/revolution and 16 microsteps driver setting,
// this means 15.6 revolutions/s

bool direction = false;

void loop() {
  if (!stepper) {
    return;
  }
#define STEPS 500
  Serial.println("Start");
  for (uint32_t step = 1; step < STEPS; step++) {
    // Ticks at start/end: 10ms
    // @step = STEPS/2: it is 10ms/STEPS for STEPS=500 => 20us
    uint32_t k = max(step, STEPS - step);
    uint32_t ticks = TICKS_PER_S / 100 / (STEPS - k);
    uint16_t curr_ticks;
    uint8_t steps;
    if (ticks > 65535) {
      curr_ticks = 32768;
      steps = 1;
    } else {
      steps = 65535 / ticks;
      curr_ticks = ticks;
    }
    ticks -= curr_ticks;
    struct stepper_command_s cmd = {
        .ticks = curr_ticks, .steps = steps, .count_up = direction};
    while (true) {
      int rc = stepper->addQueueEntry(&cmd);
      if (rc == AQE_OK) {
        break;
      }
      // adding a delay(1) causes problems
      delayMicroseconds(1000);
    }
    while (ticks > 0) {
      uint16_t curr_ticks;
      if (ticks > 65535) {
        curr_ticks = 32768;
      } else {
        curr_ticks = ticks;
      }
      ticks -= curr_ticks;
      struct stepper_command_s cmd = {
          .ticks = curr_ticks, .steps = 0, .count_up = direction};
      while (true) {
        int rc = stepper->addQueueEntry(&cmd);
        if (rc == AQE_OK) {
          break;
        }
        // adding a delay(1) causes problems
        delayMicroseconds(1000);
      }
    }
  }

  Serial.println("no more commands to be created");
  while (!stepper->isQueueEmpty()) {
  }
  Serial.println("queue is empty");

  while (stepper->isRunning()) {
  }

  Serial.println("stepper has stopped");

  delay(1000);
  direction = !direction;
}
