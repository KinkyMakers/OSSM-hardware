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

FastAccelStepper *stepper;
StepperQueue fas_queue[NUM_QUEUES];

int enable_inject_on_mark = -1;
bool enable_stepper_manage_on_interrupts = false;
bool enable_stepper_manage_on_noInterrupts = false;
bool in_manage = false;

#include "RampChecker.h"
class FastAccelStepperTest {
 public:
  void init_queue() {
    fas_queue[0]._initVars();
    fas_queue[1]._initVars();
  }
  void inject() { stepper->fill_queue(); }
  void do_test() {
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    stepper = &s;

    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    int32_t steps = 10000;

    // Increase speed to 400, then further to 300
    // Identified bug was a fast jump to 300 without acceleration
    assert(s.isQueueEmpty());
    s.setSpeedInUs(400);
    s.setAcceleration(10000);
    in_manage = true;
    s.fill_queue();
    in_manage = false;
    assert(s.isQueueEmpty());
    assert(!s.isRunning());
    s.moveTo(3000);
    assert(s.isRunning());
    in_manage = true;
    s.fill_queue();
    in_manage = false;
    assert(!s.isQueueEmpty());

    float old_planned_time_in_buffer = 0;
    int moveto_done = false;
    for (int i = 0; i < steps; i++) {
      if (!moveto_done && (s.getCurrentPosition() >= 500)) {
        moveto_done = true;
        s.moveTo(4000);
      }
      if (true) {
        printf(
            "Loop %d: Queue read/write = %d/%d    Target pos = %d, Queue End "
            "pos = %d  QueueEmpty=%s\n",
            i, fas_queue[0].read_idx, fas_queue[0].next_write_idx,
            s.targetPos(), s.getPositionAfterCommandsCompleted(),
            s.isQueueEmpty() ? "yes" : "no");
      }
      in_manage = true;
      if (!s.isRampGeneratorActive() && s.isQueueEmpty()) {
        break;
      }
      s.fill_queue();
      uint32_t from_dt = rc.total_ticks;
      while (!s.isQueueEmpty()) {
        rc.check_section(
            &fas_queue[0].entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
        fas_queue[0].read_idx++;
      }
      in_manage = false;
      uint32_t to_dt = rc.total_ticks;
      float planned_time = (to_dt - from_dt) * 1.0 / 16000000;
      printf("%d: planned time in buffer: %.6fs\n", i, planned_time);
      // This must be ensured, so that the stepper does not run out of commands
      old_planned_time_in_buffer = planned_time;
    }
    test(!s.isRampGeneratorActive(), "too many commands created");
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
    printf("Total steps = %d\n", rc.pos);
    assert(rc.pos == 4000);

    printf("TEST_05 Part PASSED\n");
  }
};

FastAccelStepperTest test;
void inject_fill_interrupt(int mark) {
  if ((mark == enable_inject_on_mark) && !in_manage) {
    in_manage = true;
    test.inject();
    in_manage = false;
  }
}
void noInterrupts() {
  if (enable_stepper_manage_on_noInterrupts && !in_manage) {
    in_manage = true;
    test.inject();
    in_manage = false;
  }
}
void interrupts() {
  if (enable_stepper_manage_on_interrupts && !in_manage) {
    in_manage = true;
    test.inject();
    in_manage = false;
  }
}
int main() {
  enable_stepper_manage_on_interrupts = false;
  enable_stepper_manage_on_noInterrupts = false;
  enable_inject_on_mark = -1;
  test.do_test();

  enable_stepper_manage_on_interrupts = false;
  enable_stepper_manage_on_noInterrupts = false;
  enable_inject_on_mark = 0;
  test.do_test();

  enable_stepper_manage_on_interrupts = false;
  enable_stepper_manage_on_noInterrupts = false;
  enable_inject_on_mark = 1;
  test.do_test();

  enable_stepper_manage_on_interrupts = false;
  enable_stepper_manage_on_noInterrupts = false;
  enable_inject_on_mark = 2;
  test.do_test();

  enable_stepper_manage_on_interrupts = false;
  enable_stepper_manage_on_noInterrupts = true;
  enable_inject_on_mark = -1;
  test.do_test();

  enable_stepper_manage_on_interrupts = true;
  enable_stepper_manage_on_noInterrupts = false;
  enable_inject_on_mark = -1;
  test.do_test();

  printf("TEST_05 PASSED\n");
  return 0;
}
