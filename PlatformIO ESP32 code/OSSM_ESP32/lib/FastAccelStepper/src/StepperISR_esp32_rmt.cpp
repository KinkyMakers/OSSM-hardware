#include "StepperISR.h"

#ifdef SUPPORT_ESP32_RMT

//#define TEST_PROBE_1 21
//#define TEST_PROBE_2 18
#ifdef TEST_PROBE_1
int tp1 = 0;
#define PROBE_1(x) digitalWrite(TEST_PROBE_1, x)
#define PROBE_1_TOGGLE \
  tp1 = 1 - tp1;       \
  digitalWrite(TEST_PROBE_1, tp1)
#else
#define PROBE_1(x)
#define PROBE_1_TOGGLE
#endif
#ifdef TEST_PROBE_2
int tp2 = 0;
#define PROBE_2(x) digitalWrite(TEST_PROBE_2, x)
#define PROBE_2_TOGGLE \
  tp2 = 2 - tp2;       \
  digitalWrite(TEST_PROBE_2, tp2)
#else
#define PROBE_2(x)
#define PROBE_2_TOGGLE
#endif

// The following concept is in use:
//
//    The buffer of 64Bytes is split into two parts à  31 words.
//    Each part will hold on command (or part of).
//    After the 2*31 words an end marker is placed.
//    The threshold is set to 31.
//
//    This way, the threshold interrupt occurs after the first part
//    and the end interrupt after the second part.
//
// Of these 32 bits, the low 16-bit entry is sent first and the high entry
// second.
#define PART_SIZE 31

static void IRAM_ATTR apply_command(StepperQueue *q, bool fill_part_one,
                                    uint32_t *data) {
  if (!fill_part_one) {
    data += PART_SIZE;
  }
  uint8_t rp = q->read_idx;
  if (rp == q->next_write_idx) {
    for (uint8_t i = 0; i < PART_SIZE; i++) {
      // two pauses à 4096 ticks
      *data++ = 0x12341234;
    }
    for (uint8_t i = 2 * PART_SIZE; i < 64; i++) {
      *data++ = 0x10001234;
    }
    RMT.conf_ch[q->channel].conf1.tx_conti_mode = 0;
    if (!q->_isRunning) {
      // second invocation to stop.
      rmt_tx_stop(q->channel);
      rmt_rx_stop(q->channel);
      rmt_memory_rw_rst(q->channel);
      q->_rmtStopped = true;
    }
    q->_isRunning = false;
    return;
  } else {
    struct queue_entry *e_curr = &q->entry[rp & QUEUE_LEN_MASK];

    uint8_t steps = e_curr->steps;
    uint16_t ticks = e_curr->ticks;
    if (steps != 0) {
      //		PROBE_2_TOGGLE;
    }
    if (steps == 0) {
      for (uint8_t i = 0; i < PART_SIZE - 1; i++) {
        // two pauses à 16 ticks
        *data++ = 0x00100010;
        ticks -= 32;
      }
      uint16_t ticks_l = ticks >> 1;
      uint16_t ticks_r = ticks - ticks_l;
      uint32_t rmt_entry = ticks_l;
      rmt_entry <<= 16;
      rmt_entry |= ticks_r;
      *data = rmt_entry;
    } else if (ticks == 0xffff) {
      // special treatment for this case, because an rmt entry can only cover up
      // to 0xfffe ticks every step must be minimum split into two rmt entries,
      // so at max PART/2 steps can be done.
      if (steps < PART_SIZE / 2) {
        for (uint8_t i = 1; i < steps; i++) {
          // steps-1 iterations
          *data++ = 0x40017fff | 0x8000;
          *data++ = 0x20002000;
        }
        *data++ = 0x40017fff | 0x8000;
        uint32_t remaining = 0x20002000;
        // 2*(steps - 1) + 1 already stored => 2*steps - 1
        // and after this for loop one entry added => 2*steps
        for (uint8_t i = 2 * steps; i < PART_SIZE; i++) {
          *data++ = 0x01000100;
          remaining -= 0x01000100;
        }
        *data = remaining;
        steps = 0;
      } else {
        steps -= PART_SIZE / 2;
        for (uint8_t i = 0; i < PART_SIZE / 2; i++) {
          *data++ = 0x40017fff | 0x8000;
          *data++ = 0x20002000;
        }
        data--;
      }
    } else if ((steps < 2 * PART_SIZE) && (steps != PART_SIZE)) {
      uint8_t steps_to_do = steps;
      if (steps > PART_SIZE) {
        steps_to_do = steps / 2;
      }

      uint16_t ticks_l = ticks >> 1;
      uint16_t ticks_r = ticks - ticks_l;
      uint32_t rmt_entry = ticks_l;
      rmt_entry <<= 16;
      rmt_entry |= ticks_r | 0x8000;  // with step
      for (uint8_t i = 1; i < steps_to_do; i++) {
        *data++ = rmt_entry;
      }
      uint32_t *last_step_data = data++;
      for (uint8_t i = steps_to_do; i < PART_SIZE; i++) {
        *data++ = 0x00040004;
        rmt_entry -= 0x00040004;
      }
      *last_step_data = rmt_entry;
      steps -= steps_to_do;
      data--;
    } else {
      // every entry one step
      uint16_t ticks_l = ticks >> 1;
      uint16_t ticks_r = ticks - ticks_l;
      uint32_t rmt_entry = ticks_l;
      rmt_entry <<= 16;
      rmt_entry |= ticks_r | 0x8000;  // with step
      for (uint8_t i = 0; i < PART_SIZE; i++) {
        *data++ = rmt_entry;
      }
      // This could lead to single step at high speed in next part .... so need
      // to rework The previous part tries to address this
      steps -= PART_SIZE;
      data--;
    }
    if (!fill_part_one) {
      // Note: When enabling the continuous transmission mode by setting
      // RMT_REG_TX_CONTI_MODE, the transmitter will transmit the data on the
      // channel continuously, that is, from the first byte to the last one,
      // then from the first to the last again, and so on. In this mode, there
      // will be an idle level lasting one clk_div cycle between N and N+1
      // transmissions.
      *data -= 1;
    }
    if (steps == 0) {
      // The command has been completed
      if (e_curr->repeat_entry == 0) {
        rp++;
      }
      q->read_idx = rp;
      // The dir pin toggle at this place is problematic, but if the last
      // command contains only one step, it could work
      if (rp == q->next_write_idx) {
        struct queue_entry *e_next = &q->entry[rp & QUEUE_LEN_MASK];
        if (e_next->toggle_dir) {
          gpio_num_t dirPin = (gpio_num_t)q->dirPin;
          gpio_set_level(dirPin, gpio_get_level(dirPin) ^ 1);
        }
      }
    } else {
      e_curr->steps = steps;
    }
  }
}

#ifndef RMT_CHANNEL_MEM
#define RMT_LIMIT tx_lim
#define RMT_FIFO apb_fifo_mask
#else
#define RMT_LIMIT limit
#define RMT_FIFO fifo_mask
#endif

#define PROCESS_CHANNEL(ch)                                                    \
  if (mask & RMT_CH##ch##_TX_END_INT_ST) {                                     \
    apply_command(&fas_queue[QUEUES_MCPWM_PCNT + ch], false, FAS_RMT_MEM(ch)); \
  }                                                                            \
  if (mask & RMT_CH##ch##_TX_THR_EVENT_INT_ST) {                               \
    apply_command(&fas_queue[QUEUES_MCPWM_PCNT + ch], true, FAS_RMT_MEM(ch));  \
    /* now repeat the interrupt at buffer size + end marker */                 \
    RMT.tx_lim_ch[ch].RMT_LIMIT = PART_SIZE * 2 + 1;                           \
  }

static void IRAM_ATTR tx_intr_handler(void *arg) {
  uint32_t mask = RMT.int_st.val;
  RMT.int_clr.val = mask;
  PROCESS_CHANNEL(0);
  PROCESS_CHANNEL(1);
#if QUEUES_RMT >= 4
  PROCESS_CHANNEL(2);
  PROCESS_CHANNEL(3);
#endif
#if QUEUES_RMT == 8
  PROCESS_CHANNEL(4);
  PROCESS_CHANNEL(5);
  PROCESS_CHANNEL(6);
  PROCESS_CHANNEL(7);
#endif
}

void StepperQueue::init_rmt(uint8_t channel_num, uint8_t step_pin) {
#ifdef TEST_PROBE_1
  pinMode(TEST_PROBE_1, OUTPUT);
  PROBE_1(LOW);
#endif
#ifdef TEST_PROBE_2
  pinMode(TEST_PROBE_2, OUTPUT);
  PROBE_2(LOW);
#endif

  _initVars();
  _step_pin = step_pin;

  channel = (rmt_channel_t)channel_num;

  if (channel_num == 0) {
    periph_module_enable(PERIPH_RMT_MODULE);
  }

  // 80 MHz/5 = 16 MHz
  rmt_set_source_clk(channel, RMT_BASECLK_APB);
  rmt_set_clk_div(channel, 5);
  rmt_set_mem_block_num(channel, 1);
  rmt_set_tx_carrier(channel, false, 0, 0, RMT_CARRIER_LEVEL_LOW);
  rmt_tx_stop(channel);
  rmt_rx_stop(channel);
  if (channel_num == 0) {
    rmt_isr_register(tx_intr_handler, NULL,
                     ESP_INTR_FLAG_SHARED | ESP_INTR_FLAG_IRAM, NULL);
    RMT.apb_conf.RMT_FIFO = 1;
    RMT.apb_conf.mem_tx_wrap_en = 0;
  }

  digitalWrite(step_pin, LOW);
  pinMode(step_pin, OUTPUT);

  connect();
}

void StepperQueue::connect_rmt() {
#ifndef __ESP32_IDF_V44__
  rmt_set_pin(channel, RMT_MODE_TX, (gpio_num_t)_step_pin);
#else
  rmt_set_gpio(channel, RMT_MODE_TX, (gpio_num_t)_step_pin, false);
#endif
  RMT.conf_ch[channel].conf1.idle_out_lv = 0;
  RMT.conf_ch[channel].conf1.idle_out_en = 1;
}

void StepperQueue::disconnect_rmt() {
  rmt_set_tx_intr_en(channel, false);
  rmt_set_tx_thr_intr_en(channel, false, 0);
#ifndef __ESP32_IDF_V44__
//  rmt_set_pin(channel, RMT_MODE_TX, (gpio_num_t)-1);
#else
  rmt_set_gpio(channel, RMT_MODE_TX, GPIO_NUM_NC, false);
#endif
  RMT.conf_ch[channel].conf1.idle_out_en = 0;
}

void StepperQueue::startQueue_rmt() {
//#define TRACE
#ifdef TRACE
  Serial.println("START");
#endif
  rmt_tx_stop(channel);
  rmt_rx_stop(channel);
  rmt_memory_rw_rst(channel);
  uint32_t *mem = FAS_RMT_MEM(channel);
  for (uint8_t i = 0; i < 64; i += 2) {
    mem[i + 0] = 0x0fff8fff;
    mem[i + 1] = 0x7fff8fff;
  }
  mem[2 * PART_SIZE] = 0;
  _isRunning = true;
  rmt_set_tx_intr_en(channel, false);
  rmt_set_tx_thr_intr_en(channel, false, 0);
  RMT.apb_conf.mem_tx_wrap_en = 0;

#ifdef TRACE
  Serial.println(next_write_idx - read_idx);
#endif

  apply_command(this, true, mem);

#ifdef TRACE
  Serial.print(RMT.conf_ch[channel].conf0.val, BIN);
  Serial.print(' ');
  Serial.print(RMT.conf_ch[channel].conf1.val, BIN);
  Serial.println(' ');
  Serial.print(RMT.apb_conf.mem_tx_wrap_en);
  Serial.println(' ');
  for (uint8_t i = 0; i < 64; i++) {
    Serial.print(i);
    Serial.print(' ');
    Serial.println(mem[i], HEX);
  }
  if (!isRunning()) {
    Serial.println("STOPPED");
  }
#endif

  apply_command(this, false, mem);

#ifdef TRACE
  Serial.print(RMT.conf_ch[channel].conf0.val, BIN);
  Serial.print(' ');
  Serial.print(RMT.conf_ch[channel].conf1.val, BIN);
  Serial.println(' ');
  Serial.print(RMT.apb_conf.mem_tx_wrap_en);
  Serial.println(' ');
  for (uint8_t i = 0; i < 64; i++) {
    Serial.print(i);
    Serial.print(' ');
    Serial.println(mem[i], HEX);
  }
  if (!isRunning()) {
    Serial.println("STOPPED");
  }
#endif
  if (_isRunning) {
    RMT.conf_ch[channel].conf1.tx_conti_mode = 1;
  }
  rmt_set_tx_thr_intr_en(channel, true, PART_SIZE + 1);
  rmt_set_tx_intr_en(channel, true);
  _rmtStopped = false;
  RMT.conf_ch[channel].conf1.tx_start = 1;
  //  RMT.conf_ch[channel].conf1.tx_start = 0;
}
void StepperQueue::forceStop_rmt() {
  // Based on finding in issue #101, the rmt module in esp32 and esp32s2 behaves
  // differently. Apparently, the esp32 rmt cannot be stopped, while esp32s2
  // can. So implement a version, which should be able to cope with both

  // try to stop the rmt module. Seems to work only on esp32s2
  rmt_tx_stop(channel);

  // esp32 will continue to run, so disable the interrupts
  rmt_set_tx_intr_en(channel, false);
  rmt_set_tx_thr_intr_en(channel, false, 0);

  // stop esp32 rmt, by let it hit the end
  RMT.conf_ch[channel].conf1.tx_conti_mode = 0;

  // replace buffer with only pauses, coming from end
  uint32_t *data = FAS_RMT_MEM(channel) + 63;
  for (uint8_t i = 0; i < 64; i++) {
    *data-- = 0x00010001;
  }

  // the queue is not running anymore
  _isRunning = false;

  // and empty the buffer
  read_idx = next_write_idx;

  // as the rmt is not running anymore, mark it as stopped
  _rmtStopped = true;
}
bool StepperQueue::isReadyForCommands_rmt() {
  if (_isRunning) {
    return true;
  }
  return _rmtStopped;
}
uint16_t StepperQueue::_getPerformedPulses_rmt() { return 0; }
#endif
