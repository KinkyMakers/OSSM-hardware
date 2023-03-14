#include "test_seq.h"

//
// This test sequence is to reproduce issue #103

bool test_seq_12(FastAccelStepper *stepper, struct test_seq_s *seq,
                 uint32_t time_ms) {
  switch (seq->state) {
    case 0:  // INIT
      stepper->setSpeedInUs(64);
      stepper->setAcceleration(1000);
      stepper->moveTo(10000000);
      seq->u32_1 = time_ms;
      seq->state++;
      break;
    case 1:
      if (stepper->getPeriodInUsAfterCommandsCompleted() == 64) {
        stepper->setAcceleration(10000);
        seq->s16_1 = 0;
        seq->s16_2 = 0;
        seq->u32_1 = time_ms;
        seq->state++;
      }
      break;
    case 2:
      if (time_ms - seq->u32_1 >= 20) {
        stepper->setSpeedInUs(64 + seq->s16_1);
        stepper->applySpeedAcceleration();
        seq->u32_1 = time_ms;
        seq->state++;
      }
      break;
    case 3:
      if (time_ms - seq->u32_1 >= 20) {
        stepper->setSpeedInUs(64);
        stepper->applySpeedAcceleration();
        seq->u32_1 = time_ms;
        if (seq->s16_2 < 10) {
          seq->s16_2++;
          seq->state = 2;
        } else {
          seq->s16_1 = -seq->s16_1;
          seq->s16_2 = 0;
          if (seq->s16_1 >= 0) {
            seq->s16_1++;
          }
          if (seq->s16_1 >= 10000) {
            seq->state = 4;
            stepper->setAcceleration(1000);
            stepper->applySpeedAcceleration();
            stepper->stopMove();
          } else {
            Serial.print("Speed changes 64us <=> ");
            Serial.print(64 + seq->s16_1);
            Serial.println("us");
            seq->state = 2;
          }
        }
      }
      break;
    case 4:
      if (!stepper->isRunning()) {
        return true;
      }
      break;
  }
  return false;
}
