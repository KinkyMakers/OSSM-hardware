class RampChecker {
 public:
  uint64_t total_ticks;
  uint32_t last_dt;
  uint32_t min_dt;
  bool increase_ok;
  bool flat_ok;
  bool decrease_ok;
  bool first;
  bool dir_high;
  bool reversing_allowed;
  uint32_t accelerate_till;
  uint32_t coast_till;
  uint32_t time_coasting;
  uint32_t pos;
  uint32_t ticks_since_last_step = 0;

  void next_ramp() {
    increase_ok = true;
    decrease_ok = false;
    last_dt = ~0;
    min_dt = ~0;
    first = true;
    dir_high = true;
    coast_till = 0;
    time_coasting = 0;
    accelerate_till = 0;
    reversing_allowed = false;
  }
  RampChecker() {
    total_ticks = 0;
    pos = 0;
    next_ramp();
  }
  void check_section(struct queue_entry *e) {
    uint8_t steps = e->steps;
    if (steps == 0) {
      // Just a pause
      if (ticks_since_last_step <= 0xffff0000) {
        ticks_since_last_step += e->ticks;
      }
      total_ticks += e->ticks;
      printf("process pause %d => %u\n", e->ticks, ticks_since_last_step);
      return;
    }
    if (e->toggle_dir) {
      assert(reversing_allowed);
      dir_high = !dir_high;
      increase_ok = true;
      last_dt = ~0;
      decrease_ok = false;
    }
    if (dir_high) {
      pos += steps;
    } else {
      pos -= steps;
    }
    uint32_t curr_dt = ticks_since_last_step;
    total_ticks += steps * e->ticks;
    if (!first) {
      min_dt = min(min_dt, curr_dt);
    }
    float accel = 0;
    if (last_dt != ~0) {
      accel = (16000000.0 / curr_dt - 16000000.0 / last_dt) /
              (1.0 / 16000000.0 * curr_dt);
    }
    printf(
        "process command in ramp checker @%.6fs: steps = %d last = %u current "
        "= %u "
        " min_dt "
        "= %d   accel=%.6f inc=%s dec=%s\n",
        total_ticks / 16000000.0, steps, last_dt, curr_dt, min_dt, accel,
        increase_ok ? "ALLOW" : "NO", decrease_ok ? "ALLOW" : "NO");

    assert(first || (steps * curr_dt > 0));

    if (last_dt > curr_dt) {
      assert(increase_ok);
      accelerate_till = total_ticks;
      decrease_ok = true;
    } else if (last_dt < curr_dt) {
      if (increase_ok) {
        coast_till = total_ticks - curr_dt;
      }
      assert(decrease_ok);
      increase_ok = false;
    } else {
      time_coasting += steps * curr_dt;
    }

    if (!first) {
      last_dt = curr_dt;
    }

    ticks_since_last_step = e->ticks;
    first = false;
  }
};
