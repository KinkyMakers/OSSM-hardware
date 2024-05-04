#include "test_seq.h"

// u32_1 shall be number of steps

bool test_seq_02(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  int32_t steps = seq->u32_1;
  switch (seq->state) {
    case 0:  // INIT
      stepper->setSpeedInUs(40);
      stepper->setAcceleration(1000);
      seq->u32_1 = 1;
      seq->state++;
      break;
    case 1:
      // Turn
      stepper->move(steps);
      seq->state++;
      break;
    case 3:
      // Turn back
      stepper->move(-steps);
      seq->state++;
      break;
    case 2:
    case 4:
      if (!stepper->isRunning()) {
        seq->state++;
      }
      break;
    case 5:
      if (seq->u32_1 >= 6400) {
        return true;  // finished
      }
      seq->u32_1++;
      seq->u32_1 += seq->u32_1 >> 2;
      seq->state = 1;
      break;
  }
  return false;
}
