#include "pinning.h"
#ifdef ARDUINO_ARCH_AVR
#include <avr/sleep.h>
#endif

FastAccelStepperEngine engine = FastAccelStepperEngine();
FastAccelStepper *stepper;

void setup() {
  Serial.begin(115200);

  engine.init();
  stepper = engine.stepperConnectToPin(stepPinStepper);
  if (stepper) {
    stepper->setDirectionPin(dirPinStepper);
    stepper->setEnablePin(enablePinStepper);
    stepper->setAutoEnable(true);
    stepper->enableOutputs();
  }

  uint16_t lframes[] = {107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
                        107, 107, 107, 107, 107, 107, 107, 107, 107, 107,
                        107, 107, 107, 107, 107, 107, 107, 107, 107, 97};
  // a representation of a 360 degree rot at 30 fps
  uint16_t lenAnim = 30;

#define TICKS_PER_FRAME (TICKS_PER_S / 30)
  for (uint16_t i = 0; i < lenAnim; i++) {  // for each frame

    uint16_t steps = lframes[i];

    // THIS WORKS ONLY, IF STEPS > 0
    uint32_t ticks_per_step = TICKS_PER_FRAME / steps;
    uint32_t this_frame_steps = 0;

    while (this_frame_steps < steps) {
      // repeat as long as not all steps for this frame created...

      if (ticks_per_step < 65535) {  // does it fit into an uint16_t ?
        // Yes, so every command will generate at least one step
        uint8_t this_cmd_steps = 1;
        if ((uint32_t)this_cmd_steps * ticks_per_step < TICKS_PER_S / 500) {
          // in order to have ~2ms long command, this command should issue
          // multiple steps
          this_cmd_steps = TICKS_PER_S / 500 / ticks_per_step;
        }
        if (this_cmd_steps + this_frame_steps > steps) {
          this_cmd_steps = steps - this_frame_steps;
        }
        struct stepper_command_s cmd_step = {.ticks = (uint16_t)ticks_per_step,
                                             .steps = this_cmd_steps,
                                             .count_up = true};

        // add command to the queue
        int rc;
        do {
          rc = stepper->addQueueEntry(&cmd_step);
          if (rc > 0) {
            // so the queue is busy => put the task to sleep for
            // portTICK_PERIOD_MS
            vTaskDelay(1);
          }
        } while (rc > 0);  // repeat addQueueEntry, if queue is busy

        // sum up the generated steps
        this_frame_steps += this_cmd_steps;
      } else {
        // For one step, one command with a step + one or several pauses are
        // needed

        // let's remember how many ticks still to be generated
        uint32_t remaining_ticks = ticks_per_step;

        // Let's first do the step
        struct stepper_command_s cmd_step = {
            .ticks = 16384, .steps = 1, .count_up = true};

        // add command to the queue
        int rc;
        do {
          rc = stepper->addQueueEntry(&cmd_step);
          if (rc > 0) {
            // so the queue is busy => put the task to sleep for
            // portTICK_PERIOD_MS
            vTaskDelay(1);
          }
        } while (rc > 0);  // repeat addQueueEntry, if queue is busy

        // sum up this one step
        this_frame_steps += 1;

        // and reduce the remaining ticks
        remaining_ticks -= 16384;

        // Now create the pauses until remaining_ticks is 0
        while (remaining_ticks > 0) {
          uint16_t this_cmd_ticks;

          // do remaining ticks fit into an uint16_t
          if (remaining_ticks > 65535) {
            // nope, so make a pause of 32768 ticks
            this_cmd_ticks = 32768;
          } else {
            // yeah, fits. So use that value
            this_cmd_ticks = (uint16_t)remaining_ticks;
          }

          // make a pause command
          struct stepper_command_s cmd_pause = {
              .ticks = this_cmd_ticks, .steps = 0, .count_up = true};

          // and add it to the queue
          int rc;
          do {
            rc = stepper->addQueueEntry(&cmd_pause);
            if (rc > 0) {
              // so the queue is busy => put the task to sleep for
              // portTICK_PERIOD_MS
              vTaskDelay(1);
            }
          } while (rc > 0);  // repeat addQueueEntry, if queue is busy

          // reduce the remaining ticks
          remaining_ticks -= this_cmd_ticks;
        }
      }
    }
  }
}

void loop() {
  Serial.println("DONE");
  Serial.println(stepper->getPositionAfterCommandsCompleted());
  //        delay(1000);
  //        noInterrupts();
  //        sleep_cpu();
}
