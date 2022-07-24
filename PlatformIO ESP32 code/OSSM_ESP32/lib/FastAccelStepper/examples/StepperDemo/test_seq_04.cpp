#include "test_seq.h"

// u32_1 shall be number of steps

bool test_seq_04(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  stepper->setSpeedInUs(40);
  stepper->setAcceleration(100000);
  seq->u32_1 = 1;
  seq->state++;
  seq->test = test_seq_02;
  return false;
}
