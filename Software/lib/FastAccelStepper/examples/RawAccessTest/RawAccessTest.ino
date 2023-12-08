#include "FastAccelStepper.h"

#ifdef SIM_TEST_INPUT
#include <avr/sleep.h>
#endif

// Exclusively for test purposes !

// for avr: either use pin 9 or 10 aka OC1A or OC1B

// platformio does not manage cpp directives well
// #define are always executed
#if defined(ARDUINO_ARCH_AVR)
const uint8_t stepPinStepper = 9;
const uint8_t enablePinStepper = 6;
const uint8_t dirPinStepper = 5;
#elif defined(ARDUINO_ARCH_ESP32)
const uint8_t stepPinStepper = 17;
const uint8_t enablePinStepper = 26;
const uint8_t dirPinStepper = 18;
#elif defined(ARDUINO_ARCH_SAM)
const uint8_t stepPinStepper = 1;  // only defined to satisfy compiler
const uint8_t enablePinStepper = 2;
const uint8_t dirPinStepper = 3;
#endif

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

    Serial.println("Start");
    uint32_t _start = micros();
    uint32_t cnt = 0;
    for (uint32_t delay = 0; delay < 20000; delay += 100) {
      // just issue a step with 1ms pause
      struct stepper_command_s cmd = {
          .ticks = 16000, .steps = 2, .count_up = true};
      int rc = stepper->addQueueEntry(&cmd);
      if (rc != AQE_OK) {
        Serial.print("Queue error:");
        Serial.println(rc);
      } else {
        cnt++;
        while (stepper->isRunning()) {
        }
      }
      uint32_t x = micros() + delay;
      while (micros() < x) {
      }
      _start += delay;
    }
    uint32_t _end = micros();
    Serial.println(_start);
    Serial.println(_end);
    Serial.println("Done. ");
    Serial.print(cnt);
    Serial.print(" Stepper commands executed within us=");
    Serial.print((_end - _start) / cnt);
    Serial.println(". Expected ~1000");
  }
}

void loop() {
  delay(1000);
#ifdef SIM_TEST_INPUT
  noInterrupts();
  sleep_cpu();
#endif
}
