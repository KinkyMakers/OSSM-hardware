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

  void ramp(uint32_t accel, uint32_t steps) {
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    // Reproduce test sequence 06

    uint32_t speed_us = 40;
    assert(s.isQueueEmpty());
    s.setSpeedInUs(speed_us);
    s.setAcceleration(accel);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.move(steps);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;

    char fname[100];
    sprintf(fname, "test_08.gnuplot");
    FILE *gp_file = fopen(fname, "w");
    fprintf(gp_file, "$data <<EOF\n");
    for (int i = 0; i < 100 * steps; i++) {
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
        rc.increase_ok = true;
        rc.decrease_ok = true;
        rc.check_section(
            &fas_queue[0].entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
        fas_queue[0].read_idx++;
        fprintf(gp_file, "%.6f %.2f %d\n", rc.total_ticks / 1000000.0,
                16000000.0 / rc.last_dt, rc.last_dt);
      }
      uint32_t to_dt = rc.total_ticks;
      float planned_time = (to_dt - from_dt) * 1.0 / 16000000;
      printf("planned time in buffer: %.6fs\n", planned_time);
      // This must be ensured, so that the stepper does not run out of commands
      assert((i == 0) || (old_planned_time_in_buffer > 0.005));
      old_planned_time_in_buffer = planned_time;
    }
    fprintf(gp_file, "EOF\n");
    fprintf(gp_file, "plot $data using 1:2 with linespoints\n");
    fprintf(gp_file, "pause -1\n");
    fclose(gp_file);
    test(!s.isRampGeneratorActive(), "too many commands created");
    test(s.getCurrentPosition() == steps, "has not reached target position");
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);

#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }
};
int main() {
  FastAccelStepperTest test;
  test.ramp(100000, 400);
  test.ramp(100000, 600);
  for (uint32_t i = 0; i < 30; i++) {
    test.ramp(1000000, 1000 + i);
  }
  printf("TEST_08 PASSED\n");
  return 0;
}
