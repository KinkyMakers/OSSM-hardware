#include "StepperISR.h"

#ifdef SUPPORT_ESP32_MCPWM_PCNT

#define DEFAULT_TIMER_H_L_TRANSITION 160

// cannot be updated while timer is running => fix it to 0
#define TIMER_PRESCALER 0

//#define TEST_PROBE 18

// Here the associated mapping from queue to mcpwm/pcnt units
//
// As the ISR is accessing this table, the mapping cannot be put into flash,
// even this is actually a constant table
struct mapping_s {
  mcpwm_unit_t mcpwm_unit;
  uint8_t timer;
  mcpwm_io_signals_t pwm_output_pin;
  pcnt_unit_t pcnt_unit;
  uint8_t input_sig_index;
  uint32_t cmpr_tea_int_clr;
  uint32_t cmpr_tea_int_ena;
  uint32_t cmpr_tea_int_raw;
};

static struct mapping_s channel2mapping[NUM_QUEUES] = {
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 0,
      pwm_output_pin : MCPWM0A,
      pcnt_unit : PCNT_UNIT_0,
      input_sig_index : PCNT_SIG_CH0_IN0_IDX,
      cmpr_tea_int_clr : MCPWM_OP0_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP0_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP0_TEA_INT_RAW
    },
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 1,
      pwm_output_pin : MCPWM1A,
      pcnt_unit : PCNT_UNIT_1,
      input_sig_index : PCNT_SIG_CH0_IN1_IDX,
      cmpr_tea_int_clr : MCPWM_OP1_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP1_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP1_TEA_INT_RAW
    },
    {
      mcpwm_unit : MCPWM_UNIT_0,
      timer : 2,
      pwm_output_pin : MCPWM2A,
      pcnt_unit : PCNT_UNIT_2,
      input_sig_index : PCNT_SIG_CH0_IN2_IDX,
      cmpr_tea_int_clr : MCPWM_OP2_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP2_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP2_TEA_INT_RAW
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 0,
      pwm_output_pin : MCPWM0A,
      pcnt_unit : PCNT_UNIT_3,
      input_sig_index : PCNT_SIG_CH0_IN3_IDX,
      cmpr_tea_int_clr : MCPWM_OP0_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP0_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP0_TEA_INT_RAW
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 1,
      pwm_output_pin : MCPWM1A,
      pcnt_unit : PCNT_UNIT_4,
      input_sig_index : PCNT_SIG_CH0_IN4_IDX,
      cmpr_tea_int_clr : MCPWM_OP1_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP1_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP1_TEA_INT_RAW
    },
    {
      mcpwm_unit : MCPWM_UNIT_1,
      timer : 2,
      pwm_output_pin : MCPWM2A,
      pcnt_unit : PCNT_UNIT_5,
      input_sig_index : PCNT_SIG_CH0_IN5_IDX,
      cmpr_tea_int_clr : MCPWM_OP2_TEA_INT_CLR,
      cmpr_tea_int_ena : MCPWM_OP2_TEA_INT_ENA,
      cmpr_tea_int_raw : MCPWM_OP2_TEA_INT_RAW
    },
};

static void IRAM_ATTR prepare_for_next_command(
    StepperQueue *queue, const struct queue_entry *e_next) {
  uint8_t next_steps = e_next->steps;
  if (next_steps > 0) {
    const struct mapping_s *mapping =
        (const struct mapping_s *)queue->driver_data;
    pcnt_unit_t pcnt_unit = mapping->pcnt_unit;
    // is updated only on zero
    PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim = next_steps;
  }
}

#define isr_pcnt_counter_clear(pcnt_unit)             \
  REG_SET_BIT(PCNT_CTRL_REG, (1 << (2 * pcnt_unit))); \
  REG_CLR_BIT(PCNT_CTRL_REG, (1 << (2 * pcnt_unit)))

static void IRAM_ATTR apply_command(StepperQueue *queue,
                                    const struct queue_entry *e) {
  const struct mapping_s *mapping =
      (const struct mapping_s *)queue->driver_data;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  pcnt_unit_t pcnt_unit = mapping->pcnt_unit;
  uint8_t timer = mapping->timer;
  uint8_t steps = e->steps;
  if (e->toggle_dir) {
    gpio_num_t dirPin = (gpio_num_t)queue->dirPin;
    gpio_set_level(dirPin, gpio_get_level(dirPin) ^ 1);
  }
  uint16_t ticks = e->ticks;
#ifndef __ESP32_IDF_V44__
  if (mcpwm->timer[timer].status.value <= 1) {  // mcpwm Timer is stopped ?
    mcpwm->timer[timer].period.upmethod = 0;    // 0 = immediate update, 1 = TEZ
#else                                           /* __ESP32_IDF_V44__ */
  if (mcpwm->timer[timer].timer_status.timer_value <=
      1) {  // mcpwm Timer is stopped ?
    mcpwm->timer[timer].timer_cfg0.timer_period_upmethod =
        0;  // 0 = immediate update, 1 = TEZ
#endif                                          /* __ESP32_IDF_V44__ */
  } else {
    // 0 = immediate update, 1 = TEZ
#ifndef __ESP32_IDF_V44__
    mcpwm->timer[timer].period.upmethod = 1;
#else  /* __ESP32_IDF_V44__ */
    mcpwm->timer[timer].timer_cfg0.timer_period_upmethod = 1;
#endif /* __ESP32_IDF_V44__ */
  }
#ifndef __ESP32_IDF_V44__
  mcpwm->timer[timer].period.period = ticks;
#else  /* __ESP32_IDF_V44__ */
  mcpwm->timer[timer].timer_cfg0.timer_period = ticks;
#endif /* __ESP32_IDF_V44__ */
  if (steps == 0) {
    // timer value = 1 - upcounting: output low
#ifndef __ESP32_IDF_V44__
    mcpwm->channel[timer].generator[0].utea = 1;
#else  /* __ESP32_IDF_V44__ */
    mcpwm->operators[timer].generator[0].gen_utea = 1;
#endif /* __ESP32_IDF_V44__ */
    mcpwm->int_clr.val = mapping->cmpr_tea_int_clr;
    mcpwm->int_ena.val |= mapping->cmpr_tea_int_ena;
  } else {
    bool disable_mcpwm_interrupt = true;
    if (PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim != steps) {
      // For fast pulses, eventually the ISR is late. So take the current pulse
      // count into consideration.
      //
      // Automatic update for next pulse cnt on pulse counter zero does not
      // work. For example the sequence:
      //		5 pulses
      //		1 pause		==> here need to store 3, but not
      // available yet 		3 pulses
      //
      // Read counter
      uint16_t val1 = steps - PCNT.cnt_unit[pcnt_unit].cnt_val;
      // Clear flag for l-->h transition
      mcpwm->int_clr.val = mapping->cmpr_tea_int_clr;
      // Read counter again
      uint16_t val2 = steps - PCNT.cnt_unit[pcnt_unit].cnt_val;
      // If no pulse arrives between val1 and val2:
      //		val2 == val1
      // If pulse arrives between val1 and int_clr:
      //		mcpwm status is cleared and val2 == val1+1
      // If pulse arrives between int_clr and val2:
      //		mcpwm status is set and val2 == val1+1
      // => mcwpm status info is not reliable, so clear again
      if (val1 != val2) {
        // Clear flag again. No pulse can be expected between val2 and here
        mcpwm->int_clr.val = mapping->cmpr_tea_int_clr;
      }

      // is updated only on zero
      PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim = val2;
      // force take over
      isr_pcnt_counter_clear(pcnt_unit);
      // Check, if pulse has come in
      if ((mcpwm->int_raw.val & mapping->cmpr_tea_int_raw) != 0) {
        // Pulse has come in
        // Check if the pulse has been counted or not
        if (PCNT.cnt_unit[pcnt_unit].cnt_val == 0) {
          // pulse hasn't been counted, so adjust the limit
          // is updated only on zero
          PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim = val2 - 1;
          // force take over
          isr_pcnt_counter_clear(pcnt_unit);

          // In case val2 == 1, then there may be a problem.
          // The pulse counter may not cause any interrupt with cnt_h_lim == 0
          // and if the mcpwm-interrupt is disabled, then the queue is stalled.
          // in this case, we should leave the mcpwm enabled
          disable_mcpwm_interrupt = val2 > 0;
        }
      }
    }
    // disable mcpwm interrupt
    if (disable_mcpwm_interrupt) {
      mcpwm->int_ena.val &= ~mapping->cmpr_tea_int_ena;
    }
    // timer value = 1 - upcounting: output high
#ifndef __ESP32_IDF_V44__
    mcpwm->channel[timer].generator[0].utea = 2;
#else  /* __ESP32_IDF_V44__ */
    mcpwm->operators[timer].generator[0].gen_utea = 2;
#endif /* __ESP32_IDF_V44__ */
  }
}

static void IRAM_ATTR init_stop(StepperQueue *q) {
  // init stop is normally called after the first command,
  // because the second command is entered too late
  // and after the last command aka running out of commands.
  const struct mapping_s *mapping = (const struct mapping_s *)q->driver_data;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;
#ifndef __ESP32_IDF_V44__
  mcpwm->timer[timer].mode.start = 0;  // 0: stop at TEZ
#else                                  /* __ESP32_IDF_V44__ */
  mcpwm->timer[timer].timer_cfg1.timer_start = 0;  // 0: stop at TEZ
#endif                                 /* __ESP32_IDF_V44__ */
  // timer value = 1 - upcounting: output low
  mcpwm->int_ena.val &= ~mapping->cmpr_tea_int_ena;
  // PCNT.conf_unit[mapping->pcnt_unit].conf2.cnt_h_lim = 1;
  q->_isRunning = false;
}

static void IRAM_ATTR what_is_next(StepperQueue *q) {
  // when starting the queue, apply_command for the first entry is called.
  // the read pointer stays at this position. This function is reached,
  // if the command to which read pointer points to is completed.
  bool isPrepared = q->_nextCommandIsPrepared;
  q->_nextCommandIsPrepared = false;
  uint8_t rp = q->read_idx;
  if (rp != q->next_write_idx) {
    struct queue_entry *e_completed = &q->entry[rp & QUEUE_LEN_MASK];
    if (e_completed->repeat_entry == 0) {
      rp++;
      q->read_idx = rp;
    }
    if (rp != q->next_write_idx) {
      struct queue_entry *e_curr = &q->entry[rp & QUEUE_LEN_MASK];
      if (!isPrepared) {
        prepare_for_next_command(q, e_curr);  // a no-op for pause command
        const struct mapping_s *mapping =
            (const struct mapping_s *)q->driver_data;
        isr_pcnt_counter_clear(mapping->pcnt_unit);
      }
      apply_command(q, e_curr);
      rp++;
      if (rp != q->next_write_idx) {
        struct queue_entry *e_next = &q->entry[rp & QUEUE_LEN_MASK];
        q->_nextCommandIsPrepared = true;
        prepare_for_next_command(q, e_next);  // a no-op for pause command
      }
      return;
    }
  }
  // no more commands: stop timer at period end
  init_stop(q);
}

static void IRAM_ATTR pcnt_isr_service(void *arg) {
  StepperQueue *q = (StepperQueue *)arg;
  what_is_next(q);
}

// MCPWM_SERVICE is only used in case of pause
#ifndef __ESP32_IDF_V44__

#define MCPWM_SERVICE(mcpwm, TIMER, pcnt)             \
  if (mcpwm.int_st.cmpr##TIMER##_tea_int_st != 0) {   \
    /*managed in apply_command()                   */ \
    /*mcpwm.int_clr.cmpr##TIMER##_tea_int_clr = 1;*/  \
    StepperQueue *q = &fas_queue[pcnt];               \
    what_is_next(q);                                  \
  }

#else /* __ESP32_IDF_V44__ */

#define MCPWM_SERVICE(mcpwm, TIMER, pcnt)             \
  if (mcpwm.int_st.op##TIMER##_tea_int_st != 0) {     \
    /*managed in apply_command()                   */ \
    /*mcpwm.int_clr.cmpr##TIMER##_tea_int_clr = 1;*/  \
    StepperQueue *q = &fas_queue[pcnt];               \
    what_is_next(q);                                  \
  }

#endif /* __ESP32_IDF_V44__ */

static void IRAM_ATTR mcpwm0_isr_service(void *arg) {
  // For whatever reason, this interrupt is constantly called even with int_st =
  // 0 while the timer is running
  MCPWM_SERVICE(MCPWM0, 0, 0);
  MCPWM_SERVICE(MCPWM0, 1, 1);
  MCPWM_SERVICE(MCPWM0, 2, 2);
}
static void IRAM_ATTR mcpwm1_isr_service(void *arg) {
  MCPWM_SERVICE(MCPWM1, 0, 3);
  MCPWM_SERVICE(MCPWM1, 1, 4);
  MCPWM_SERVICE(MCPWM1, 2, 5);
}

void StepperQueue::init_mcpwm_pcnt(uint8_t channel_num, uint8_t step_pin) {
#ifdef TEST_PROBE
  pinMode(TEST_PROBE, OUTPUT);
#endif

  _initVars();
  _step_pin = step_pin;

  const struct mapping_s *mapping = &channel2mapping[channel_num];
  driver_data = (void *)mapping;

  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  pcnt_unit_t pcnt_unit = mapping->pcnt_unit;
  uint8_t timer = mapping->timer;

  pcnt_config_t cfg;
  // if step_pin is not set here (or 0x30), then it does not work
  cfg.pulse_gpio_num = step_pin;          // static 0 is 0x30, static 1 is 0x38
  cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;  // static 0 is 0x30, static 1 is 0x38
  cfg.lctrl_mode = PCNT_MODE_KEEP;
  cfg.hctrl_mode = PCNT_MODE_KEEP;
  cfg.pos_mode = PCNT_COUNT_INC;  // increment on rising edge
  cfg.neg_mode = PCNT_COUNT_DIS;  // ignore falling edge
  cfg.counter_h_lim = 1;
  cfg.counter_l_lim = 0;
  cfg.unit = pcnt_unit;
  cfg.channel = PCNT_CHANNEL_0;
  pcnt_unit_config(&cfg);

  PCNT.conf_unit[pcnt_unit].conf2.cnt_h_lim = 1;
  PCNT.conf_unit[pcnt_unit].conf0.thr_h_lim_en = 1;
  PCNT.conf_unit[pcnt_unit].conf0.thr_l_lim_en = 0;

  pcnt_counter_clear(pcnt_unit);
  pcnt_counter_resume(pcnt_unit);
  pcnt_event_enable(pcnt_unit, PCNT_EVT_H_LIM);
  if (channel_num == 0) {
    // isr_service_install apparently enables the interrupt
    PCNT.int_clr.val = PCNT.int_st.val;
    pcnt_isr_service_install(ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM);
  }
  pcnt_isr_handler_add(pcnt_unit, pcnt_isr_service, (void *)this);

  if (timer == 0) {
    // Init mcwpm module for use
    periph_module_enable(mcpwm_unit == MCPWM_UNIT_0 ? PERIPH_PWM0_MODULE
                                                    : PERIPH_PWM1_MODULE);
    mcpwm->int_ena.val = 0;  // disable all interrupts
    mcpwm_isr_register(
        mcpwm_unit,
        mcpwm_unit == MCPWM_UNIT_0 ? mcpwm0_isr_service : mcpwm1_isr_service,
        NULL, ESP_INTR_FLAG_IRAM | ESP_INTR_FLAG_SHARED, NULL);

    // 160 MHz/5 = 32 MHz => 16 MHz in up/down-mode
#ifndef __ESP32_IDF_V44__
    mcpwm->clk_cfg.prescale = 5 - 1;
#else  /* __ESP32_IDF_V44__ */
    mcpwm->clk_cfg.clk_prescale = 5 - 1;
#endif /* __ESP32_IDF_V44__ */

    // timer 0 is input for operator 0
    // timer 1 is input for operator 1
    // timer 2 is input for operator 2
#ifndef __ESP32_IDF_V44__
    mcpwm->timer_sel.operator0_sel = 0;
    mcpwm->timer_sel.operator1_sel = 1;
    mcpwm->timer_sel.operator2_sel = 2;
#else  /* __ESP32_IDF_V44__ */
    mcpwm->operator_timersel.operator0_timersel = 0;
    mcpwm->operator_timersel.operator1_timersel = 1;
    mcpwm->operator_timersel.operator2_timersel = 2;
#endif /* __ESP32_IDF_V44__ */
  }
#ifndef __ESP32_IDF_V44__
  mcpwm->timer[timer].period.upmethod = 1;  // 0 = immediate update, 1 = TEZ
  mcpwm->timer[timer].period.prescale = TIMER_PRESCALER;
  mcpwm->timer[timer].period.period = 400;  // Random value
  mcpwm->timer[timer].mode.mode = 3;        // 3=up/down counting
  mcpwm->timer[timer].mode.start = 0;       // 0: stop at TEZ
#else                                       /* __ESP32_IDF_V44__ */
  // 0 = immediate update, 1 = TEZ
  mcpwm->timer[timer].timer_cfg0.timer_period_upmethod = 1;
  mcpwm->timer[timer].timer_cfg0.timer_prescale = TIMER_PRESCALER;
  mcpwm->timer[timer].timer_cfg0.timer_period = 400;  // Random value
  mcpwm->timer[timer].timer_cfg1.timer_mod = 3;       // 3=up/down counting
  mcpwm->timer[timer].timer_cfg1.timer_start = 0;     // 0: stop at TEZ
#endif                                      /* __ESP32_IDF_V44__ */

  // this sequence should reset the timer to 0
#ifndef __ESP32_IDF_V44__
  mcpwm->timer[timer].sync.timer_phase = 0;  // prepare value of 0
  mcpwm->timer[timer].sync.in_en = 1;        // enable sync
  mcpwm->timer[timer].sync.sync_sw ^= 1;     // force a sync
  mcpwm->timer[timer].sync.in_en = 0;        // disable sync

  mcpwm->channel[timer].cmpr_cfg.a_upmethod = 0;     // 0 = immediate update
  mcpwm->channel[timer].cmpr_value[0].cmpr_val = 1;  // set compare value A
  mcpwm->channel[timer].generator[0].val = 0;   // clear all trigger actions
  mcpwm->channel[timer].generator[1].val = 0;   // clear all trigger actions
  mcpwm->channel[timer].generator[0].dtep = 1;  // low at period
  mcpwm->channel[timer].db_cfg.val = 0;         // edge delay disabled
  mcpwm->channel[timer].carrier_cfg.val = 0;    // carrier disabled
#else                                           /* __ESP32_IDF_V44__ */
  mcpwm->timer[timer].timer_sync.timer_phase = 0;     // prepare value of 0
  mcpwm->timer[timer].timer_sync.timer_synci_en = 1;  // enable sync
  mcpwm->timer[timer].timer_sync.timer_sync_sw ^= 1;  // force a sync
  mcpwm->timer[timer].timer_sync.timer_synci_en = 0;  // disable sync

  mcpwm->operators[timer].gen_stmp_cfg.gen_a_upmethod =
      0;                                         // 0 = immediate update
  mcpwm->operators[timer].timestamp[0].gen = 1;  // set compare value A
  mcpwm->operators[timer].generator[0].val = 0;  // clear all trigger actions
  mcpwm->operators[timer].generator[1].val = 0;  // clear all trigger actions
  mcpwm->operators[timer].generator[0].gen_dtep = 1;  // low at period
  mcpwm->operators[timer].dt_cfg.val = 0;             // edge delay disabled
  mcpwm->operators[timer].carrier_cfg.val = 0;        // carrier disabled
#endif                                          /* __ESP32_IDF_V44__ */

  digitalWrite(step_pin, LOW);
  pinMode(step_pin, OUTPUT);

  // at last, link mcpwm to output pin and back into pcnt input
  connect();
}

void StepperQueue::connect_mcpwm_pcnt() {
  const struct mapping_s *mapping = (const struct mapping_s *)driver_data;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_gpio_init(mcpwm_unit, mapping->pwm_output_pin, _step_pin);
  // Doesn't work with gpio_matrix_in
  //  gpio_matrix_in(step_pin, mapping->input_sig_index, false);
  gpio_iomux_in(_step_pin, mapping->input_sig_index);
}

void StepperQueue::disconnect_mcpwm_pcnt() {
  // sig_index = 0x100 => cancel output
  gpio_matrix_out(_step_pin, 0x100, false, false);
  // untested alternative:
  //	gpio_reset_pin((gpio_num_t)q->step_pin);
}

// Mechanism is like this, starting from stopped motor:
//
// *	init counter
// *	init mcpwm
// *	start mcpwm
// *	-- pcnt counter counts every L->H-transition at mcpwm.timer = 1
// *	-- if counter reaches planned steps, then counter is reset and
// *	interrupt is created
//
// *	pcnt interrupt: available time is from mcpwm.timer = 1+x to period
//		-	read next commmand: store period in counter shadow and
// steps in pcnt
//		- 	without next command: set mcpwm to stop mode on reaching
// period
//

void StepperQueue::startQueue_mcpwm_pcnt() {
#ifdef TEST_PROBE
  // The time used by this command can have an impact
  digitalWrite(TEST_PROBE, digitalRead(TEST_PROBE) == HIGH ? LOW : HIGH);
#endif
  const struct mapping_s *mapping = (const struct mapping_s *)driver_data;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;

  // apply_command() assumes the pcnt counter to contain executed steps
  // and deduct this from the new command. For a starting motor
  // need to make sure, that the counter is 0. (issue #33)
  pcnt_unit_t pcnt_unit = mapping->pcnt_unit;
  pcnt_counter_clear(pcnt_unit);

  _isRunning = true;
  _nextCommandIsPrepared = false;
  struct queue_entry *e = &entry[read_idx & QUEUE_LEN_MASK];
  apply_command(this, e);

#ifndef __ESP32_IDF_V44__
  mcpwm->timer[timer].mode.start = 2;  // 2=run continuous
#else                                  /* __ESP32_IDF_V44__ */
  mcpwm->timer[timer].timer_cfg1.timer_start = 2;     // 2=run continuous
#endif                                 /* __ESP32_IDF_V44__ */
}
void StepperQueue::forceStop_mcpwm_pcnt() {
  init_stop(this);
  read_idx = next_write_idx;
}
bool StepperQueue::isReadyForCommands_mcpwm_pcnt() {
  if (isRunning()) {
    return true;
  }
  const struct mapping_s *mapping = (const struct mapping_s *)driver_data;
  mcpwm_unit_t mcpwm_unit = mapping->mcpwm_unit;
  mcpwm_dev_t *mcpwm = mcpwm_unit == MCPWM_UNIT_0 ? &MCPWM0 : &MCPWM1;
  uint8_t timer = mapping->timer;
#ifndef __ESP32_IDF_V44__
  if (mcpwm->timer[timer].status.value > 1) {
#else  /* __ESP32_IDF_V44__ */
  if (mcpwm->timer[timer].timer_status.timer_value > 1) {
#endif /* __ESP32_IDF_V44__ */
    return false;
  }
  return true;
  //#ifndef __ESP32_IDF_V44__
  //  return (mcpwm->timer[timer].mode.start != 2);  // 2=run continuous
  //#else                                            /* __ESP32_IDF_V44__ */
  //  return (mcpwm->timer[timer].timer_cfg1.timer_start != 2);  // 2=run
  //  continuous
  //#endif                                           /* __ESP32_IDF_V44__ */
}
uint16_t StepperQueue::_getPerformedPulses_mcpwm_pcnt() {
  const struct mapping_s *mapping = (const struct mapping_s *)driver_data;
  return PCNT.cnt_unit[mapping->pcnt_unit].cnt_val;
}

#endif
