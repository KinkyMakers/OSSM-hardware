#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "FastAccelStepper.h"
#include "StepperISR.h"

char TCCR1A;
char TCCR1B;
char TCCR1C;
char TIMSK1;
char TIFR1;
unsigned short OCR1A;
unsigned short OCR1B;

StepperQueue fas_queue[NUM_QUEUES];

void inject_fill_interrupt(int mark) {}
void noInterrupts() {}
void interrupts() {}

#include "RampChecker.h"

class FastAccelStepperTest {
 public:
  void init_queue() {
    fas_queue[0].read_idx = 0;
    fas_queue[1].read_idx = 0;
    fas_queue[0].next_write_idx = 0;
    fas_queue[1].next_write_idx = 0;
  }

  void reduce_speed() {
    puts("Test test_speed_decrease");
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    int32_t steps = 100;

    // M1 A1000 V10000 P100 w300 V100000 U
    assert(s.isQueueEmpty());
    s.setAcceleration(1000);
    s.setSpeedInUs(10000);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.moveTo(100);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;
    int speed_decreased = false;
    uint32_t count_state_dec = 0;
    for (int i = 0; i < steps * 10; i++) {
      if (!speed_decreased && (s.getCurrentPosition() >= 35)) {
        puts("Change speed");
        speed_decreased = true;
        s.setSpeedInUs(100000);
        s.applySpeedAcceleration();
        s.fill_queue();  // ensure queue is not empty
      }
      if (speed_decreased && (s.getCurrentPosition() >= 90)) {
        test((s.rampState() & RAMP_STATE_MASK) == RAMP_STATE_COAST,
             "Coasting is required state here");
        break;
      }
      if (true) {
        printf(
            "Loop %d: Queue read/write = %d/%d    Target pos = %d, Queue End "
            "pos = %d  QueueEmpty=%s\n",
            i, fas_queue[0].read_idx, fas_queue[0].next_write_idx,
            s.targetPos(), s.getPositionAfterCommandsCompleted(),
            s.isQueueEmpty() ? "yes" : "no");
      }
      if (!s.isRampGeneratorActive()) {
        break;
      }
      s.fill_queue();
      if ((s.rampState() & RAMP_STATE_MASK) == RAMP_STATE_DECELERATE) {
        count_state_dec++;
      }
      uint32_t from_dt = rc.total_ticks;
      while (!s.isQueueEmpty()) {
        rc.check_section(
            &fas_queue[0].entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
        fas_queue[0].read_idx++;
      }
      uint32_t to_dt = rc.total_ticks;
      float planned_time = (to_dt - from_dt) * 1.0 / 16000000;
      printf("planned time in buffer: %.6fs\n", planned_time);
      // This must be ensured, so that the stepper does not run out of commands
      assert((i == 0) || (old_planned_time_in_buffer > 0.005));
      old_planned_time_in_buffer = planned_time;
    }
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }
};

int main() {
  FastAccelStepperTest test;
  test.reduce_speed();
  printf("TEST_11 PASSED\n");
  return 0;
}
