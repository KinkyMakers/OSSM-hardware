#include "test_seq.h"

//
// This test sequence is to reproduce issue #103

bool test_seq_13(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  uint8_t res;
  struct stepper_command_s cmd_step = {
      .ticks = 50000, .steps = 1, .count_up = true};
  switch (seq->state) {
    case 0:  // INIT
      stepper->setAutoEnable(false);
      stepper->enableOutputs();
    case 1:
    case 2:
    case 3:
      res = stepper->addQueueEntry(&cmd_step);
      if (res > 1) {
        Serial.print(res);
        Serial.print(' ');
      } else {
        seq->u32_1 = time_ms;
        seq->state++;
      }
      break;
    case 4:
      if (time_ms - seq->u32_1 >= 20) {
        if (stepper->getCurrentPosition() != 4) {
          Serial.println("not all raw commands executed");
          seq->state = TEST_STATE_ERROR;
        }
        return true;
      }
      break;
  }
  return false;
}
