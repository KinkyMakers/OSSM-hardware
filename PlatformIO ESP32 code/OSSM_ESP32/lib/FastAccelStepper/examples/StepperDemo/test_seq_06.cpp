#include "test_seq.h"

// u32_1 shall be milliseconds from last tick
//
// Run 320000 steps with speed changes every 100ms
// in order to reproduce issue #24

#define SPEED_1_US 100
#define SPEED_2_US 90
#define SPEED_HOME_US 100

bool test_seq_06(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  if (stepper->getPositionAfterCommandsCompleted() > 32000) {
    seq->state = TEST_STATE_ERROR;
    return true;
  }
  switch (seq->state) {
    case 0:  // INIT
      stepper->setSpeedInUs(SPEED_1_US);
      stepper->setAcceleration(10000);
      stepper->move(32000);
      seq->u32_1 = time_ms;
      seq->state++;
      break;
    case 1:
      if (time_ms - seq->u32_1 >= 100) {
        stepper->setSpeedInUs(SPEED_2_US);
        stepper->applySpeedAcceleration();
        seq->u32_1 = time_ms;
        seq->state++;
      }
      break;
    case 2:
      if (time_ms - seq->u32_1 >= 100) {
        stepper->setSpeedInUs(SPEED_1_US);
        stepper->applySpeedAcceleration();
        seq->u32_1 = time_ms;
        seq->state--;
      }
      if (!stepper->isRunning()) {
        seq->state = 3;
      }
      break;
    case 3:  // Return to start position
      stepper->setSpeedInUs(SPEED_HOME_US);
      stepper->move(-32000);
      seq->state++;
      break;
    case 4:
      if (!stepper->isRunning()) {
        return true;  // finished
      }
      break;
  }
  return false;
}
