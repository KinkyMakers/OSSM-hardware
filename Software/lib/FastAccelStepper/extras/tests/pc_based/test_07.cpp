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

  void do_test(uint64_t dt) {
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    // Reproduce test sequence 06

    assert(s.getDirectionPin() == PIN_UNDEFINED);
    uint32_t speed_us = 100;
    int32_t steps = 32000;
    assert(s.isQueueEmpty());
    s.setSpeedInUs(speed_us);
    s.setAcceleration(10000);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.move(steps);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;
    uint64_t next_speed_change = dt;
    uint64_t mid_point_ticks = 0;

    char fname[100];
    sprintf(fname, "test_07.gnuplot");
    FILE *gp_file = fopen(fname, "w");
    fprintf(gp_file, "$data <<EOF\n");
    for (int i = 0; i < 10 * steps; i++) {
      if (rc.total_ticks > next_speed_change) {
        next_speed_change += TICKS_PER_S / 10;
        speed_us = 190 - speed_us;
        printf("Change speed to %d\n", speed_us);
        s.setSpeedInUs(speed_us);
        s.applySpeedAcceleration();
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
        if ((mid_point_ticks == 0) && (rc.pos >= steps / 2)) {
          mid_point_ticks = rc.total_ticks;
        }
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
      test(s.getCurrentPosition() <= steps, "has overshot");
    }
    fprintf(gp_file, "EOF\n");
    fprintf(gp_file, "plot $data using 1:2 with linespoints\n");
    fprintf(gp_file, "pause -1\n");
    fclose(gp_file);
    test(!s.isRampGeneratorActive(), "too many commands created");
    test(s.getCurrentPosition() == steps, "has not reached target position");
    printf("Total time  %f\n", rc.total_ticks / 16000000.0);

    printf("Time coasting = %d\n", rc.time_coasting);
    test(rc.time_coasting < 46000000, "too much coasting");

    printf("mid point @ %ld => total = %ld, total ticks = %ld\n",
           mid_point_ticks, 2 * mid_point_ticks, rc.total_ticks);
#define ALLOWED_ASYMMETRY 1000000L
    printf("%ld\n", ALLOWED_ASYMMETRY);
    test(mid_point_ticks * 2 < rc.total_ticks + ALLOWED_ASYMMETRY,
         "ramp is not symmetric 1");
    test(mid_point_ticks * 2 > rc.total_ticks - ALLOWED_ASYMMETRY,
         "ramp is not symmetric 2");

#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }
};

int main() {
  FastAccelStepperTest test;
  for (uint64_t time_shift = 0; time_shift < TICKS_PER_S / 10;
       time_shift += TICKS_PER_S / 7000) {
    test.do_test(time_shift);
  }
  printf("TEST_07 PASSED\n");
  return 0;
}
