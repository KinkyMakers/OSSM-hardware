#include <stdint.h>

#include "FastAccelStepper.h"
#include "StepperISR.h"

#include "RampConstAcceleration.h"
#include "common.h"

#ifdef SUPPORT_UPM_TIMER_FREQ_VARIABLES
static upm_float upm_timer_freq;
static upm_float upm_timer_freq_div_500;
static upm_float upm_timer_freq_div_sqrt_of_2;
static upm_float upm_timer_freq_square_div_2;
#endif

void init_ramp_module() {
#ifdef SUPPORT_UPM_TIMER_FREQ_VARIABLES
  upm_timer_freq = upm_from((uint32_t)TICKS_PER_S);
  upm_timer_freq_div_500 = upm_divide(upm_timer_freq, UPM_CONST_500);
  upm_timer_freq_div_sqrt_of_2 =
      upm_shr(upm_multiply(upm_timer_freq, upm_timer_freq));
  upm_timer_freq_square_div_2 = upm_shr(upm_square(upm_timer_freq));
#endif
}

//*************************************************************************************************

//#define TRACE
#ifdef TRACE
#define TRACE_OUTPUT(x) Serial.print(x)
#else
#define TRACE_OUTPUT(x)
#endif

#ifdef TEST
void print_ramp_state(uint8_t this_state) {
  switch (this_state & RAMP_DIRECTION_MASK) {
    case RAMP_DIRECTION_COUNT_UP:
      printf("+");
      break;
    case RAMP_DIRECTION_COUNT_DOWN:
      printf("-");
      break;
  }
  switch (this_state & RAMP_STATE_MASK) {
    case RAMP_STATE_COAST:
      printf("COAST");
      break;
    case RAMP_STATE_ACCELERATE:
      printf("ACC");
      break;
    case RAMP_STATE_DECELERATE:
      printf("DEC");
      break;
    case RAMP_STATE_DECELERATE_TO_STOP:
      printf("STOP");
      break;
    case RAMP_STATE_REVERSE:
      printf("REVERSE");
      break;
  }
}
#endif

//*************************************************************************************************
void _getNextCommand(const struct ramp_ro_s *ramp, const struct ramp_rw_s *rw,
                     const struct queue_end_s *queue_end,
                     NextCommand *command) {
  {
    // If there is a pause from last step, then just output a pause
    uint32_t pause_ticks = rw->pause_ticks_left;
    if (pause_ticks > 0) {
      if (pause_ticks > 65535) {
        pause_ticks >>= 1;
        pause_ticks = fas_min(pause_ticks, 65535);
      }
      command->command.ticks = pause_ticks;
      command->command.steps = 0;
      command->command.count_up = queue_end->count_up;
      command->rw = *rw;
      command->rw.pause_ticks_left -= pause_ticks;
#ifdef TEST
      printf("add command pause ticks = %d  remaining pause = %d\n",
             pause_ticks, command->rw.pause_ticks_left);
#endif
      return;
    }
  }
  // If the acceleration has changed, recalculate the ramp up/down steps,
  // which is the equivalent to the current speed.
  // Even if the acceleration value is constant, the calculated value
  // can deviate due to precision or clipping effect
  uint32_t curr_ticks = rw->curr_ticks;
  uint32_t performed_ramp_up_steps;
  if (ramp->config.accel_change_cnt != rw->accel_change_cnt) {
    TRACE_OUTPUT('X');
    if (curr_ticks == TICKS_FOR_STOPPED_MOTOR) {
      performed_ramp_up_steps = 0;
    } else {
      performed_ramp_up_steps = upm_to_u32(upm_multiply(
          ramp->config.upm_inv_accel2, upm_rsquare(upm_from(curr_ticks))));
#ifdef TEST
      printf(
          "Recalculate performed_ramp_up_steps from %d to %d from %d ticks\n",
          rw->performed_ramp_up_steps, performed_ramp_up_steps, curr_ticks);
#endif
    }
  } else {
    performed_ramp_up_steps = rw->performed_ramp_up_steps;
  }

  bool count_up = queue_end->count_up;

  // check state for acceleration/deceleration or deceleration to stop
  uint8_t this_state;
  uint32_t remaining_steps;
  bool need_count_up;
  if (ramp->keep_running) {
    need_count_up = ramp->keep_running_count_up;
    remaining_steps = 0xfffffff;
  } else {
    // this can overflow, which is legal
    int32_t delta = ramp->target_pos - queue_end->pos;

    if (delta == 0) {
      // this case can happen on overshoot. So reverse current direction
      need_count_up = !count_up;
    } else {
      need_count_up = delta > 0;
    }
    remaining_steps = fas_abs(delta);
  }

#ifdef TRACE
  if (remaining_steps == performed_ramp_up_steps) {
    Serial.print('=');
  } else if (remaining_steps > performed_ramp_up_steps) {
    uint32_t dx = remaining_steps - performed_ramp_up_steps;
    if (dx < 10) {
      char ch = '0' + dx;
      Serial.print(ch);
      Serial.print('x');
    }
  } else {
    uint32_t dx = performed_ramp_up_steps - remaining_steps;
    if (dx < 10) {
      char ch = '0' + dx;
      Serial.print(ch);
      Serial.print('y');
    }
  }
#endif

  // If not moving, then use requested direction
  if (performed_ramp_up_steps == 0) {
    count_up = need_count_up;
  }

  if ((remaining_steps == 0) && (performed_ramp_up_steps <= 1)) {
    command->command.ticks = 0;
    command->rw.pause_ticks_left = 0;
    command->rw.ramp_state = RAMP_STATE_IDLE;
    command->rw.curr_ticks = TICKS_FOR_STOPPED_MOTOR;
#ifdef TEST
    puts("ramp complete");
#endif
    return;
  }

  // Forward planning of 2ms or more on slow speed.
  uint16_t planning_steps;
  if (curr_ticks < TICKS_PER_S / 1000) {
    upm_float upm_ps =
        upm_divide(UPM_TICKS_PER_S_DIV_500, upm_from(curr_ticks));
    planning_steps = upm_to_u16(upm_ps);
  } else {
    planning_steps = 1;
  }
  uint16_t orig_planning_steps = planning_steps;

  // In case of force stop just run down the ramp
  if (ramp->force_stop) {
    this_state = RAMP_STATE_DECELERATE_TO_STOP;
    remaining_steps = performed_ramp_up_steps;
  } else if (count_up != need_count_up) {
    // On direction change, do reversing
    this_state = RAMP_STATE_REVERSE;
    remaining_steps = performed_ramp_up_steps;
  } else {
    // If come here, then direction is same as current movement
    if (remaining_steps == performed_ramp_up_steps) {
      this_state = RAMP_STATE_DECELERATE;
      // remaining_steps = performed_ramp_up_steps;
    } else if (remaining_steps < performed_ramp_up_steps) {
      // We will overshoot
      TRACE_OUTPUT('O');
      this_state = RAMP_STATE_REVERSE;
      remaining_steps = performed_ramp_up_steps;
    } else if (ramp->config.min_travel_ticks < rw->curr_ticks) {
      this_state = RAMP_STATE_ACCELERATE;
      if (rw->curr_ticks < 2 * MIN_CMD_TICKS) {
        // special consideration needed, that invalid commands are not generated
        //
        // possible coast steps is divided by 4: 1 part acc, 2 part coast, 1
        // part dec
        uint32_t possible_coast_steps =
            (remaining_steps - performed_ramp_up_steps) >> 2;
        // curr_ticks is not necessarily correct due to speed increase
        uint32_t coast_time = possible_coast_steps * rw->curr_ticks;
        if (coast_time < 2 * MIN_CMD_TICKS) {
          TRACE_OUTPUT('l');
          this_state = RAMP_STATE_COAST;
#ifdef TEST
          printf("low speed coast %d %d\n", possible_coast_steps,
                 remaining_steps - performed_ramp_up_steps);
#endif
        }
        if (planning_steps > remaining_steps - performed_ramp_up_steps) {
          this_state = RAMP_STATE_DECELERATE;
        }
      } else if (remaining_steps - performed_ramp_up_steps <
                 2 * planning_steps) {
        if (curr_ticks != TICKS_FOR_STOPPED_MOTOR) {
          this_state = RAMP_STATE_COAST;
          planning_steps = remaining_steps - performed_ramp_up_steps;
        }
      }
    } else if (ramp->config.min_travel_ticks > rw->curr_ticks) {
      TRACE_OUTPUT('d');
      this_state = RAMP_STATE_DECELERATE;
      if (performed_ramp_up_steps <= planning_steps) {
        if (performed_ramp_up_steps > 0) {
          planning_steps = performed_ramp_up_steps;
        } else {
          planning_steps = 1;
        }
      }
    } else {
      TRACE_OUTPUT('c');
      this_state = RAMP_STATE_COAST;
      uint32_t possible_coast_steps = remaining_steps - performed_ramp_up_steps;
      if (possible_coast_steps < 2 * planning_steps) {
        planning_steps = possible_coast_steps;
        if (curr_ticks < MIN_CMD_TICKS) {
          uint32_t cmd_ticks = curr_ticks * planning_steps;
          if (cmd_ticks < MIN_CMD_TICKS) {
            this_state = RAMP_STATE_DECELERATE;
          }
        }
      }
    }
  }
  if (remaining_steps == 0) {  // This implies performed_ramp_up_steps == 0
    command->command.ticks = 0;
    command->rw.pause_ticks_left = 0;
    command->rw.ramp_state = RAMP_STATE_IDLE;
    command->rw.curr_ticks = TICKS_FOR_STOPPED_MOTOR;
#ifdef TEST
    puts("ramp complete");
#endif
    return;
  }

  // Guarantee here:
  //	remaining_steps > 0
  //	remaining_steps >= performed_ramp_up_steps
  //	remaining_steps >  performed_ramp_up_steps, in COAST
  //	performed_ramp_up_steps can be 0
  //	planning_steps >= 1
#ifdef TEST
  assert(remaining_steps > 0);
  assert(remaining_steps >= performed_ramp_up_steps);
  assert((remaining_steps > performed_ramp_up_steps) ||
         (this_state != RAMP_STATE_COAST));
  assert(planning_steps > 0);
#endif

#ifdef TEST
  printf("prus=%d planning_steps=%d remaining_steps=%d force_stop=%d\n",
         performed_ramp_up_steps, planning_steps, remaining_steps,
         ramp->force_stop);
#endif
  uint32_t d_ticks_new;
  {
    if (this_state & RAMP_STATE_ACCELERATING_FLAG) {
      TRACE_OUTPUT('A');
      // do not overshoot ramp down start
      //
      // seems to be not necessary, as consideration already done above
      uint32_t dec_steps = remaining_steps - performed_ramp_up_steps;
      if (dec_steps < 512) {
        // Only allow half, cause the steps accelerating need to decelerate, too
        uint16_t dec_steps_u16 = (uint16_t)dec_steps;
        dec_steps_u16 /= 2;
        // Perhaps it would be better to coast instead
        // consideration has been done above already
        if (dec_steps_u16 < orig_planning_steps) {
          planning_steps = dec_steps_u16;
        }
      }

      uint32_t rs = performed_ramp_up_steps + planning_steps;
      d_ticks_new = calculate_ticks_v8(rs, ramp->config.upm_sqrt_inv_accel);
#ifdef TEST
      printf("Calculate d_ticks_new=%d from ramp steps=%d\n", d_ticks_new, rs);
#endif

      // if acceleration is very high, then d_ticks_new can be lower than
      // min_travel_ticks
      if (d_ticks_new < ramp->config.min_travel_ticks) {
        d_ticks_new = ramp->config.min_travel_ticks;
      }
    } else if (this_state & RAMP_STATE_DECELERATING_FLAG) {
      uint32_t rs;
      TRACE_OUTPUT('D');
      if (performed_ramp_up_steps == 1) {
        d_ticks_new = ramp->config.min_travel_ticks;
      } else {
        if (performed_ramp_up_steps <= planning_steps) {
          rs = planning_steps;
        } else {
          rs = performed_ramp_up_steps - planning_steps;
        }
        d_ticks_new = calculate_ticks_v8(rs, ramp->config.upm_sqrt_inv_accel);
      }
#ifdef TEST
      printf("Calculate d_ticks_new=%d from ramp steps=%d\n", d_ticks_new, rs);
#endif
    } else {
      TRACE_OUTPUT('C');
      d_ticks_new = rw->curr_ticks;
      // do not overshoot ramp down start
      uint32_t coast_steps = remaining_steps - performed_ramp_up_steps;
      if (coast_steps < 256) {
        uint16_t coast_steps_u16 = coast_steps;
        if (coast_steps_u16 < 2 * orig_planning_steps) {
          planning_steps = coast_steps_u16;
        }
      }
#ifdef TEST
      printf("planning steps=%d remaining steps=%d\n", planning_steps,
             remaining_steps);
#endif
    }
  }

  // The above plannings_steps evaluation uses curr_ticks,
  // but new_ticks can be lower and so the command time not sufficient
  if (d_ticks_new < MIN_CMD_TICKS) {
    uint32_t cmd_ticks = d_ticks_new * planning_steps;
    if (cmd_ticks < MIN_CMD_TICKS) {
      // planning_steps = MIN_CMD_TICKS / next_ticks;

      uint32_t new_planning_steps = upm_to_u32(upm_divide(
          upm_from((uint32_t)(MIN_CMD_TICKS)), upm_from(d_ticks_new)));
      new_planning_steps += new_planning_steps >> 1;
#ifdef TEST
      printf("Increase planning steps %d => %d\n", planning_steps,
             new_planning_steps);
#endif
      planning_steps = new_planning_steps;

      // do we need to decelerate in order to not overshoot ?
      if (remaining_steps < performed_ramp_up_steps + planning_steps) {
        this_state = RAMP_STATE_DECELERATE;

        // and now the speed is actually too high....
      }
    }
  }

  // perform clipping with current ticks
  uint32_t next_ticks = d_ticks_new;
  if (curr_ticks != TICKS_FOR_STOPPED_MOTOR) {
    if (this_state & RAMP_STATE_ACCELERATING_FLAG) {
      next_ticks = fas_min(next_ticks, curr_ticks);
    } else if (this_state & RAMP_STATE_DECELERATING_FLAG) {
      // CLIPPING: avoid reduction unless curr_ticks indicates stopped motor
      // Issue #25: root cause is, that curr_ticks can be
      // TICKS_FOR_STOPPED_MOTOR for the case, that queue is emptied before
      // the next command is issued
      next_ticks = fas_max(next_ticks, curr_ticks);
      //      if (this_state != RAMP_STATE_DECELERATE) {
      //        next_ticks = fas_max(next_ticks, ramp->config.min_travel_ticks);
      //      }
    }
  }
#ifdef TEST
  assert(next_ticks > 0);
#endif
#ifdef TEST
  if (next_ticks != d_ticks_new) {
    printf(
        "Clipping result d_ticks_new=%d => next_ticks=%d  with curr_ticks=%d  "
        "state=%d\n",
        d_ticks_new, next_ticks, curr_ticks, this_state);
  }
#endif

#ifdef TEST
  printf("planning steps=%d remaining steps=%d\n", planning_steps,
         remaining_steps);
#endif
  // Number of steps to execute with limitation to min 1 and max remaining steps
  uint16_t steps = planning_steps;
  steps = fas_min(steps, remaining_steps);  // This could be problematic
  steps = fas_max(steps, 1);
  steps = fas_min(255, steps);

  // Check if pauses need to be added. If yes, reduce next_ticks and calculate
  // pause_ticks_left
  uint32_t pause_ticks_left;
  if (next_ticks > 65535) {
    steps = 1;
    pause_ticks_left = next_ticks;
    next_ticks >>= 1;
    next_ticks = fas_min(next_ticks, 65535);
    pause_ticks_left -= next_ticks;
  } else {
    pause_ticks_left = 0;
  }

  // determine performed_ramp_up_steps after command enqueued
  if (this_state & RAMP_STATE_ACCELERATING_FLAG) {
    performed_ramp_up_steps += steps;
  } else if (this_state & RAMP_STATE_DECELERATING_FLAG) {
    if (performed_ramp_up_steps < steps) {
      // This can occur with performed_ramp_up_steps = 0 and steps = 1
#ifdef TEST
      assert((performed_ramp_up_steps == 0) && (steps == 1));
#endif
      // based on above assumption actually obsolete
      performed_ramp_up_steps = 0;
    } else {
      performed_ramp_up_steps -= steps;
    }
  }

  if (count_up) {
    this_state |= RAMP_DIRECTION_COUNT_UP;
  } else {
    this_state |= RAMP_DIRECTION_COUNT_DOWN;
  }

  command->command.ticks = next_ticks;
  command->command.steps = steps;
  command->command.count_up = count_up;

  command->rw.ramp_state = this_state;
  command->rw.accel_change_cnt = ramp->config.accel_change_cnt;
  command->rw.performed_ramp_up_steps = performed_ramp_up_steps;
  command->rw.pause_ticks_left = pause_ticks_left;
  command->rw.curr_ticks = pause_ticks_left + next_ticks;

#ifdef TEST
  printf(
      "pos@queue_end=%d remaining=%u ramp steps=%u planning steps=%d "
      "last_ticks=%u travel_ticks=%u ",
      queue_end->pos, remaining_steps, performed_ramp_up_steps, planning_steps,
      rw->curr_ticks, ramp->config.min_travel_ticks);
  print_ramp_state(this_state);
  printf("\n");
  printf(
      "add command Steps=%u ticks=%u  Target pos=%u "
      "Remaining steps=%u, planning_steps=%u, "
      "d_ticks_new=%u, pause_left=%u\n",
      steps, next_ticks, ramp->target_pos, remaining_steps, planning_steps,
      d_ticks_new, pause_ticks_left);
  if ((this_state & RAMP_STATE_MASK) == RAMP_STATE_ACCELERATE) {
    assert(pause_ticks_left + next_ticks >= ramp->config.min_travel_ticks);
  }
#endif
}
