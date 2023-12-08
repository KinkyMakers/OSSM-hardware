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

  void with_empty_queue() {
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    RampChecker rc = RampChecker();
    assert(0 == s.getCurrentPosition());

    assert(s.isQueueEmpty());
    s.setSpeedInUs(10000);
    s.setAcceleration(100);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.move(1000);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    for (int i = 0; i < 1000; i++) {
      if (false) {
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
      while (!s.isQueueEmpty()) {
        rc.check_section(
            &fas_queue_A.entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
        fas_queue[0].read_idx++;
      }
    }
    test(!s.isRampGeneratorActive(), "too many commands created");
    printf("min_dt=%u\n", rc.min_dt);
    test(rc.min_dt == 160000, "max speed not reached");
  }

  void with_pars(const char *name, int32_t steps, uint32_t travel_dt,
                 uint32_t accel, bool reach_max_speed, float min_time,
                 float max_time, float allowed_ramp_time_delta,
                 bool call_moveTo_repeatedly = false,
                 bool call_setAccelertion_repeatedly = false,
                 bool alternatingAccelerationValue = false,
                 bool reversing_allowed = false) {
    printf("Test %s test_with_pars steps=%d travel_dt=%d accel=%d dir=%s\n",
           name, steps, travel_dt, accel, reach_max_speed ? "CW" : "CCW");
    init_queue();
    FastAccelStepper s = FastAccelStepper();
    s.init(NULL, 0, 0);
    s.setDirectionPin(0);
    RampChecker rc = RampChecker();
    rc.reversing_allowed = reversing_allowed;
    assert(0 == s.getCurrentPosition());

    assert(s.isQueueEmpty());
    assert(0 == s.setSpeedInUs(travel_dt));
    s.setAcceleration(accel);
    s.fill_queue();
    assert(s.isQueueEmpty());
    s.move(steps);
    s.fill_queue();
    assert(!s.isQueueEmpty());
    float old_planned_time_in_buffer = 0;
    char fname[100];
    sprintf(fname, "test_02_%s.gnuplot", name);
    FILE *gp_file = fopen(fname, "w");
    fprintf(gp_file, "$data <<EOF\n");
    for (int i = 0; i < steps * 100; i++) {
      if (call_moveTo_repeatedly) {
        s.moveTo(steps);
      }
      if (call_setAccelertion_repeatedly) {
        if (alternatingAccelerationValue) {
          s.setAcceleration(accel + (i & 1) * 100);
        } else {
          s.setAcceleration(accel);
        }
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
            &fas_queue_A.entry[fas_queue[0].read_idx & QUEUE_LEN_MASK]);
        fas_queue[0].read_idx++;
        fprintf(gp_file, "%.6f %.2f %d\n", rc.total_ticks / 1000000.0,
                16000000.0 / rc.last_dt, rc.last_dt);
      }
      uint32_t to_dt = rc.total_ticks;
      float planned_time = (to_dt - from_dt) * 1.0 / 16000000;
      printf("planned time in buffer: %.6fs (old=%.6fs)\n", planned_time,
             old_planned_time_in_buffer);
      // This must be ensured, so that the stepper does not run out of commands
      assert((i == 0) || (old_planned_time_in_buffer > 0.005));
      old_planned_time_in_buffer = planned_time;
    }
    fprintf(gp_file, "EOF\n");
    fprintf(gp_file, "plot $data using 1:2 with linespoints\n");
    fprintf(gp_file, "pause -1\n");
    fclose(gp_file);
    printf("TEST=%s\n", name);
    test(!s.isRampGeneratorActive(), "too many commands created");
    printf("Total time  %f < %f < %f ?\n", min_time,
           rc.total_ticks / 16000000.0, max_time);
    test(rc.total_ticks / 16000000.0 > min_time, "ramp too fast");
    test(rc.total_ticks / 16000000.0 < max_time, "ramp too slow");
    if (reach_max_speed) {
      printf("%d = %d ?\n", rc.min_dt, travel_dt * 16);
      test(rc.min_dt == travel_dt * 16, "max speed not reached");
    } else {
      printf("%d > %d ?\n", rc.min_dt, travel_dt * 16);
      test(rc.min_dt > travel_dt * 16, "max speed reached");
    }
    float up_time, down_time;
    if (reach_max_speed) {
      printf("Ramp time up/coast/down/total=");
      up_time = 1.0 * rc.accelerate_till / 16000000.0;
      down_time = (1.0 * rc.total_ticks - 1.0 * rc.coast_till) / 16000000.0;
      printf(" %f", 1.0 * rc.accelerate_till / 16000000.0);
      printf(" %f", 1.0 * (rc.coast_till - rc.accelerate_till) / 16000000.0);
      printf(" %f", 1.0 * (rc.total_ticks - rc.coast_till) / 16000000.0);
      printf(" %f\n", 1.0 * rc.total_ticks / 16000000.0);
      assert(rc.total_ticks > rc.coast_till);
    } else {
      printf("Ramp time up/down/total =");
      up_time = 1.0 * rc.accelerate_till / 16000000.0;
      down_time =
          (1.0 * rc.total_ticks - 1.0 * rc.accelerate_till) / 16000000.0;
      printf(" %f", 1.0 * rc.accelerate_till / 16000000.0);
      printf(" %f", 1.0 * (rc.total_ticks - rc.accelerate_till) / 16000000.0);
      printf(" %f\n", 1.0 * rc.total_ticks / 16000000.0);
    }
    // turned off
    // test(abs(up_time - down_time) <
    //         0.5 * (up_time + down_time) * allowed_ramp_time_delta,
    //     "assymmetric ramp");
#if (TEST_CREATE_QUEUE_CHECKSUM == 1)
    printf("CHECKSUM for %d/%d/%d: %d\n", steps, travel_dt, accel, s.checksum);
#endif
  }
};

int main() {
  FastAccelStepperTest test;

  float nc = 0.0;  // if new ramp calculation is enabled

  test.with_empty_queue();
  //             steps  ticks_us  accel    maxspeed  min/max_total_time
  // jumps in speed in real on esp32
  test.with_pars("f1", 1000, 4300, 10000, true, 4.5 - 0.2, 4.5 + 0.2, 0.5, true,
                 true);

  // ramp 2*2s, 2*200 steps, coasting: 9600steps, 48s
  test.with_pars("f2", 10000, 5000, 100, true, 2 * 2.0 + 48.0 - 0.2 - 0.4 * nc,
                 2 * 2.0 + 48.0 + 0.2, 0.2);
  // ramp 2*0.02s, 2*2 steps, coasting: 1596 steps, 7.98s
  test.with_pars("f3", 1600, 5000, 10000, true, 7.94, 8.02, 0.2);
  // ramp 2*0.2s, 2*20 steps, coasting: 1560 steps, 7.8s
  test.with_pars("f4", 1600, 5000, 1000, true, 2 * 0.2 + 7.8 - 0.1,
                 2 * 0.2 + 7.8 + 0.1, 0.2);
  // ramp 2*1s, 5000 steps, coasting: 5000steps, 0.5s
  test.with_pars("f5", 15000, 100, 10000, true, 2 * 1.0 + 0.5 - 0.1,
                 2 * 1.0 + 0.5 + 0.1, 0.2);
  // ramp 2*0.02s, 2*2 steps, coasting: 96steps, 0.48
  test.with_pars("f6", 100, 5000, 10000, true, 2 * 0.02 + 0.48 - 0.02,
                 2 * 0.02 + 0.48 + 0.02, 0.2);
  // ramp 2s, 20000 steps => only ramp 2*0.4s
  test.with_pars("f7", 1600, 50, 10000, false, 2 * 0.4 - 0.02 - 0.4 * nc,
                 2 * 0.4 + 0.02, 0.2);
  // ramp 2*4s, 2*8000 steps, coasting 112000steps, 28s
  test.with_pars("f8", 128000, 250, 1000, true, 2 * 4.0 + 28.0 - 0.1 - 0.1 * nc,
                 2 * 4.0 + 28.0 + 0.1, 0.2);
  // ramp 2*4s, 2*8000 steps, coasting 56000steps, 14s
  test.with_pars("f9", 72000, 250, 1000, true, 2 * 4.0 + 14.0 - 0.1 - 0.1 * nc,
                 2 * 4.0 + 14.0 + 0.1, 0.2);
  // ramp 2*4s, 2*8000 steps, coasting 28000steps, 7s
  test.with_pars("f10", 44000, 250, 1000, true, 2 * 4.0 + 7.0 - 0.1 - 0.1 * nc,
                 2 * 4.0 + 7.0 + 0.1, 0.2);
  // ramp 2*4s, 2*8000 steps, coasting 2steps, 0.0005s
  test.with_pars("f11", 16000, 250, 1000, true, 2 * 4.0 + 0.0 - 0.1 - 0.1 * nc,
                 2 * 4.0 + 0.0 + 0.1, 0.2);
  // ramp 2*50s => 2*1s
  test.with_pars("f12", 1000, 20, 1000, false, 2 * 1.0 - 0.15, 2 * 1.0 + 0.1,
                 0.2);

  // The following five ramps are too fast.
  // The first step should come after ~0.6s and
  // the second after 0.89s. Implementation issues first step immediately
  // with pause to 2nd step of 0.36s (actually 0.315s).
  // So the first steps are issued within 0.36s instead of 0.89s.
  //
  // The implementation issues in addition the last two steps with 0.315s pause
  float rd = 0.7;  // rd  means ramp deviation
  //
  // ramp 2*50s, thus with 500steps max speed not reached. 250steps need 10s
  test.with_pars("f13", 500, 4000, 5, false, 20.0 - rd - 0.1 - 1.4 * nc,
                 20.0 - rd + 0.1, 0.2);
  test.with_pars("f14", 2000, 4000, 5, false, 40.0 - rd - 0.1 - 1.7 * nc,
                 40.0 - rd + 0.1, 0.2);
  // ramp 2*50s with 2*6250 steps => 100 steps at max speed using 0.4s
  test.with_pars("f15", 12600, 4000, 5, true, 100.0 + 0.4 - 0.3 - rd - 2.3 * nc,
                 100.0 + 0.4 - rd + 0.1, 0.2);
  // ramp 2*50s with 2*6250 steps => 4000 steps at max speed using 16s
  test.with_pars("f16", 16500, 4000, 5, true, 116.0 - 0.3 - rd - 2.2 * nc,
                 116.0 + 0.1 - rd, 0.2);
  // slow ramp: 2*50steps, 2*10s
  rd = 1.4;
  test.with_pars("f17", 100, 40, 1, false, 20.0 - 0.1 - rd - 2.0 * nc,
                 20.0 + 0.1 - rd, 1.0);

  // jumps in speed in real => WORKS NOW
  test.with_pars("f18", 256000, 40, 5000, true, 15.2 - 0.1, 15.2 + 0.2, 0.2);

  // ramp time  625s, 7812500 steps
  // test.with_pars("f19", 2000000, 40, 40, false, 2*223.0, 2*223.0);

  // name, steps, travel_dt, accel, reach_max_speed, min_time, max_time,
  // allowed_ramp_time_delta slow ramp time Those are anomalies (see github
  // issue #8) on avr, but not on PC
  // test.with_pars("f20", 50000, 270000, 10, true, 62.0, 63.0, 1.0);
  test.with_pars("f20", 10, 1000000, 1, true, 9.9, 10.1, 1.0);

  // no ramp time, just constant run time
  test.with_pars("f21", 15000, 4000, 100000, true, 50.9, 60.1, 0.1);
  test.with_pars("f22", 14634, 4100, 100000, true, 50.9, 60.1, 0.1);

  // single step
  test.with_pars("f23", 1, 100, 1000, false, 0.02, 0.05, 0.1);

  // try to identify issue #40
  test.with_pars("f24a", 5000, 200, 9999, true, 1.48 + 0.1 * nc, 1.5 + 0.1 * nc,
                 0.1, true, true, false, true);
  test.with_pars("f24b", 5000, 200, 9999, true, 1.48 - 0.04 * nc,
                 1.50 - 0.04 * nc, 0.1, false, false, false);
  test.with_pars("f24c", 5000, 200, 9999, true, 1.48 - 0.04 * nc,
                 1.50 - 0.04 * nc, 0.1, false, false, true);
  test.with_pars("f24d", 5000, 200, 9999, true, 1.48 - 0.04 * nc,
                 1.50 - 0.04 * nc, 0.1, true, false, true);

  test.with_pars("f25", 1000, 40, 0x7fffffff, true, 0.039, 0.041, 0.1);

  printf("TEST_02 PASSED\n");
  return 0;
}
