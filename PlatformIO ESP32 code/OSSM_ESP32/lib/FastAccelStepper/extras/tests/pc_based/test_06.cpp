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

  void do_test1() {
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    int32_t steps = 15000;

    // This sequence caused no stop:
    //	M1 N A100000 V4000 R15000
    //	V4300 U
    //	S

    assert(s.isQueueEmpty());
    s.setSpeedInUs(4000);
    s.setAcceleration(100000);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.move(steps);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;
    bool speed_changed = false;
    bool stop_initiated = false;
    for (int i = 0; i < steps; i++) {
      if (!speed_changed && (s.getCurrentPosition() >= 1000)) {
        puts("Change speed to 4300us");
        s.fill_queue();  // ensure queue is not empty
        speed_changed = true;
        s.setSpeedInUs(4300);
        s.applySpeedAcceleration();
      }
      if (!stop_initiated && (s.getCurrentPosition() >= 2000)) {
        puts("Init stop");
        s.fill_queue();  // ensure queue is not empty
        stop_initiated = true;
        s.stopMove();
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
    test(!s.isRampGeneratorActive(), "too many commands created");
    test(s.getCurrentPosition() != steps, "has not stopped");
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }

  void do_test2(uint32_t stop_at_position) {
    printf("do_test2 with stop at %d\n", stop_at_position);
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    int32_t steps = 15000;

    // This sequence does not run to position 9999:
    //	M1 N A100 V100 P9999 w100 S W P9999

    assert(s.isQueueEmpty());
    s.setSpeedInUs(100);
    s.setAcceleration(100);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.moveTo(9999);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;
    bool stop_initiated = false;
    bool restarted = false;
    for (int i = 0; i < steps; i++) {
      if (!stop_initiated && (s.getCurrentPosition() >= stop_at_position)) {
        puts("Init stop");
        s.fill_queue();  // ensure queue is not empty
        stop_initiated = true;
        s.stopMove();
      }
      if (true) {
        printf(
            "Loop %d: Queue read/write = %d/%d    Target pos = %d, Queue End "
            "pos = %d  QueueEmpty=%s\n",
            i, fas_queue[0].read_idx, fas_queue[0].next_write_idx,
            s.targetPos(), s.getPositionAfterCommandsCompleted(),
            s.isQueueEmpty() ? "yes" : "no");
      }
      s.fill_queue();
      uint32_t from_dt = rc.total_ticks;
      if (!s.isQueueEmpty()) {
        while (!s.isQueueEmpty()) {
          rc.check_section(
              &fas_queue[0].entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
          fas_queue[0].read_idx++;
        }
        uint32_t to_dt = rc.total_ticks;
        float planned_time = (to_dt - from_dt) * 1.0 / 16000000;
        printf("planned time in buffer: %.6fs\n", planned_time);
        // This must be ensured, so that the stepper does not run out of
        // commands
        assert((i == 0) || (old_planned_time_in_buffer > 0.005));
        old_planned_time_in_buffer = planned_time;
      }
      if (!s.isRampGeneratorActive()) {
        if (restarted) {
          break;
        }
        puts("Continue move to end position");
        rc.next_ramp();
        restarted = true;
        s.moveTo(9999);
      }
    }
    printf("do_test2 with stop at %d\n", stop_at_position);
    test(!s.isRampGeneratorActive(), "too many commands created");
    printf("getCurrentPosition() = %d\n", s.getCurrentPosition());
    test(s.getCurrentPosition() == 9999, "has not reached end position");
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }
};

int main() {
  FastAccelStepperTest test;
  test.do_test1();
  test.do_test2(100);
  test.do_test2(5000);
  test.do_test2(9000);
  printf("TEST_06 PASSED\n");
  return 0;
}
