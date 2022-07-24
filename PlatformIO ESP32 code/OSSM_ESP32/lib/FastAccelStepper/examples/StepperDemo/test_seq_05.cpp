#include "test_seq.h"

// u32_1 shall be milliseconds from last tick
//
// Perform 800 times a single step and then 800 steps back in one command.

#define DT 200
bool test_seq_05(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  switch (seq->state & 15) {
    case 0:  // INIT
      stepper->setSpeedInUs(40);
      stepper->setAcceleration(1000000);
      seq->u32_1 = time_ms;
      seq->state++;
      break;
    case 1:  // Wait 1s pass
      if (time_ms - seq->u32_1 >= DT) {
        if ((seq->state & 0xfff0) == (16 * 800)) {
          seq->state = 7;
          stepper->setSpeedInUs(1000);
          stepper->setAcceleration(10000);
          stepper->move(-800);
          break;
        }
        seq->state++;
      }
      break;
    case 2:
      // second pass, run motor
      seq->u32_1 += DT;
      stepper->move(1);
      seq->state += 16 - 1;
      break;
    case 7:
      if (!stepper->isRunning()) {
        return true;  // finished
      }
      break;
  }
  return false;
}
