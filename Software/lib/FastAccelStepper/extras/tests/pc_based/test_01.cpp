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

void init_queue() {
  fas_queue[0].read_idx = 0;
  fas_queue[0].next_write_idx = 0;
  fas_queue[1].read_idx = 0;
  fas_queue[1].next_write_idx = 0;
}

void basic_test() {
  puts("basic_test...");
  init_queue();
  FastAccelStepper s = FastAccelStepper();
  assert(0 == s.getCurrentPosition());
  assert(s.isQueueEmpty());
  assert(s.isQueueEmpty());
  struct stepper_command_s cmd = {
      .ticks = 10000, .steps = 100, .count_up = true};
  int res = s.addQueueEntry(&cmd);
  assert(res == AQE_OK);
  assert(!s.isQueueEmpty());
}

void queue_full() {
  puts("queue_full...");
  init_queue();
  FastAccelStepper s = FastAccelStepper();
  s.init(NULL, 0, 0);
  assert(0 == s.getCurrentPosition());
  assert(s.isQueueEmpty());
  assert(s.isQueueEmpty());
  printf("Queue read/write = %d/%d\n", fas_queue[0].read_idx,
         fas_queue[0].next_write_idx);
  struct stepper_command_s cmd = {
      .ticks = 10000, .steps = 100, .count_up = true};
  for (int i = 0; i < QUEUE_LEN - 1; i++) {
    s.addQueueEntry(&cmd);
    assert(!s.isQueueEmpty());
    assert(!s.isQueueFull());
    printf("Queue read/write = %d/%d\n", fas_queue[0].read_idx,
           fas_queue[0].next_write_idx);
  }
  s.addQueueEntry(&cmd);
  printf("Queue read/write = %d/%d\n", fas_queue[0].read_idx,
         fas_queue[0].next_write_idx);
  assert(!s.isQueueEmpty());
  assert(s.isQueueFull());
  puts("...done");
}

void queue_out_of_range() {
  int8_t res;

  puts("queue_out_of_range...");
  init_queue();
  FastAccelStepper s = FastAccelStepper();
  s.init(NULL, 0, 0);
  assert(s.isQueueEmpty());
  assert(0 == s.getCurrentPosition());
  assert(s.isQueueEmpty());
  assert(s.isQueueEmpty());

  uint16_t ticks = s.getMaxSpeedInTicks();
  ticks = ticks - 1;
  struct stepper_command_s cmd2 = {
      .ticks = ticks, .steps = 255, .count_up = true};

  res = s.addQueueEntry(&cmd2);
  test(res == AQE_ERROR_TICKS_TOO_LOW, "Too low ticks should trigger an error");
  assert(s.isQueueEmpty());

  struct stepper_command_s cmd3 = {
      .ticks = MIN_CMD_TICKS - 1, .steps = 1, .count_up = true};

  res = s.addQueueEntry(&cmd3);
  test(res == AQE_ERROR_TICKS_TOO_LOW,
       "Too short command time should trigger an error");
  assert(s.isQueueEmpty());
}

void end_pos_test() {
  init_queue();
  FastAccelStepper s = FastAccelStepper();
  s.init(NULL, 0, 0);
  assert(0 == s.getPositionAfterCommandsCompleted());
  struct stepper_command_s cmd = {.ticks = 65535, .steps = 1, .count_up = true};

  assert(AQE_OK == s.addQueueEntry(&cmd));
  assert(1 == s.getPositionAfterCommandsCompleted());
}

int main() {
  // assert(sizeof(struct queue_entry) == 6);
  basic_test();
  queue_out_of_range();
  queue_full();
  end_pos_test();
  printf("TEST_01 PASSED\n");
}
