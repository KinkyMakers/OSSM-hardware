
#include "StepperISR.h"

#if defined(SUPPORT_ESP32)

#ifdef __ESP32_IDF_V44__
#include <driver/periph_ctrl.h>
#include <soc/periph_defs.h>
#endif /* __ESP32_IDF_V44__ */

// Here are the global variables to interface with the interrupts
StepperQueue fas_queue[NUM_QUEUES];

void StepperQueue::init(uint8_t queue_num, uint8_t step_pin) {
  uint8_t channel = queue_num;
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  if (channel < 6) {
    use_rmt = false;
    init_mcpwm_pcnt(channel, step_pin);
    return;
  }
  channel -= 6;
#endif
#ifdef SUPPORT_ESP32_RMT
  use_rmt = true;
  init_rmt(channel, step_pin);
#endif
}

void StepperQueue::connect() {
#ifdef SUPPORT_ESP32_RMT
  if (use_rmt) {
    connect_rmt();
    return;
  }
#endif
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  connect_mcpwm_pcnt();
#endif
}

void StepperQueue::disconnect() {
#ifdef SUPPORT_ESP32_RMT
  if (use_rmt) {
    disconnect_rmt();
    return;
  }
#endif
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  disconnect_mcpwm_pcnt();
#endif
}

bool StepperQueue::isReadyForCommands() {
#if defined(SUPPORT_ESP32_RMT) && defined(SUPPORT_ESP32_MCPWM_PCNT)
  if (use_rmt) {
    return isReadyForCommands_rmt();
  }
  return isReadyForCommands_mcpwm_pcnt();
#elif defined(SUPPORT_ESP32_MCPWM_PCNT)
  return isReadyForCommands_mcpwm_pcnt();
#elif defined(SUPPORT_ESP32_RMT)
  return isReadyForCommands_rmt();
#else
#error "Nothing defined here"
#endif
}

void StepperQueue::startQueue() {
#ifdef SUPPORT_ESP32_RMT
  if (use_rmt) {
    startQueue_rmt();
    return;
  }
#endif
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  startQueue_mcpwm_pcnt();
#endif
}
void StepperQueue::forceStop() {
#ifdef SUPPORT_ESP32_RMT
  if (use_rmt) {
    forceStop_rmt();
    return;
  }
#endif
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  forceStop_mcpwm_pcnt();
#endif
}
uint16_t StepperQueue::_getPerformedPulses() {
#ifdef SUPPORT_ESP32_RMT
  if (use_rmt) {
    return _getPerformedPulses_rmt();
  }
#endif
#ifdef SUPPORT_ESP32_MCPWM_PCNT
  return _getPerformedPulses_mcpwm_pcnt();
#endif
  return 0;
}

//*************************************************************************************************

bool StepperQueue::isValidStepPin(uint8_t step_pin) {
  gpio_drive_cap_t strength;
  esp_err_t res = gpio_get_drive_capability((gpio_num_t)step_pin, &strength);
  return res == ESP_OK;
}
int8_t StepperQueue::queueNumForStepPin(uint8_t step_pin) { return -1; }

//*************************************************************************************************
void StepperTask(void *parameter) {
  FastAccelStepperEngine *engine = (FastAccelStepperEngine *)parameter;
  const TickType_t delay_4ms =
      (DELAY_MS_BASE + portTICK_PERIOD_MS - 1) / portTICK_PERIOD_MS;
  while (true) {
    engine->manageSteppers();
    esp_task_wdt_reset();
    vTaskDelay(delay_4ms);
  }
}

void StepperQueue::adjustSpeedToStepperCount(uint8_t steppers) {
  max_speed_in_ticks = 80;  // This equals 200kHz @ 16MHz
}

void fas_init_engine(FastAccelStepperEngine *engine, uint8_t cpu_core) {
#define STACK_SIZE 1000
#define PRIORITY configMAX_PRIORITIES
  if (cpu_core > 1) {
    xTaskCreate(StepperTask, "StepperTask", STACK_SIZE, engine, PRIORITY, NULL);
  } else {
    xTaskCreatePinnedToCore(StepperTask, "StepperTask", STACK_SIZE, engine,
                            PRIORITY, NULL, cpu_core);
  }
}

#if defined(SUPPORT_ESP32_PULSE_COUNTER)
uint32_t sig_idx[8] = {PCNT_SIG_CH0_IN0_IDX, PCNT_SIG_CH0_IN1_IDX,
                       PCNT_SIG_CH0_IN2_IDX, PCNT_SIG_CH0_IN3_IDX,
                       PCNT_SIG_CH0_IN4_IDX, PCNT_SIG_CH0_IN5_IDX,
                       PCNT_SIG_CH0_IN6_IDX, PCNT_SIG_CH0_IN7_IDX};
uint32_t ctrl_idx[8] = {PCNT_CTRL_CH0_IN0_IDX, PCNT_CTRL_CH0_IN1_IDX,
                        PCNT_CTRL_CH0_IN2_IDX, PCNT_CTRL_CH0_IN3_IDX,
                        PCNT_CTRL_CH0_IN4_IDX, PCNT_CTRL_CH0_IN5_IDX,
                        PCNT_CTRL_CH0_IN6_IDX, PCNT_CTRL_CH0_IN7_IDX};

bool _esp32_attachToPulseCounter(uint8_t pcnt_unit, FastAccelStepper *stepper,
                                 int16_t low_value, int16_t high_value) {
  // TODO: Check if free pulse counter

  pcnt_config_t cfg;
  uint8_t dir_pin = stepper->getDirectionPin();
  cfg.pulse_gpio_num = stepper->getStepPin();
  if (dir_pin == PIN_UNDEFINED) {
    cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    cfg.hctrl_mode = PCNT_MODE_KEEP;
    cfg.lctrl_mode = PCNT_MODE_KEEP;
  } else {
    cfg.ctrl_gpio_num = dir_pin;
    if (stepper->directionPinHighCountsUp()) {
      cfg.lctrl_mode = PCNT_MODE_REVERSE;
      cfg.hctrl_mode = PCNT_MODE_KEEP;
    } else {
      cfg.lctrl_mode = PCNT_MODE_KEEP;
      cfg.hctrl_mode = PCNT_MODE_REVERSE;
    }
  }
  cfg.pos_mode = PCNT_COUNT_INC;  // increment on rising edge
  cfg.neg_mode = PCNT_COUNT_DIS;  // ignore falling edge
  cfg.counter_h_lim = high_value;
  cfg.counter_l_lim = low_value;
  cfg.unit = (pcnt_unit_t)pcnt_unit;
  cfg.channel = PCNT_CHANNEL_0;
  pcnt_unit_config(&cfg);

  PCNT.conf_unit[cfg.unit].conf0.thr_h_lim_en = 0;
  PCNT.conf_unit[cfg.unit].conf0.thr_l_lim_en = 0;

  stepper->detachFromPin();
  stepper->reAttachToPin();
  gpio_iomux_in(stepper->getStepPin(), sig_idx[pcnt_unit]);
  if (dir_pin != PIN_UNDEFINED) {
    gpio_matrix_out(stepper->getDirectionPin(), 0x100, false, false);
    gpio_iomux_in(stepper->getDirectionPin(), ctrl_idx[pcnt_unit]);
    pinMode(stepper->getDirectionPin(), OUTPUT);
  }

  pcnt_counter_clear(cfg.unit);
  pcnt_counter_resume(cfg.unit);
  return true;
}
void _esp32_clearPulseCounter(uint8_t pcnt_unit) {
  pcnt_counter_clear((pcnt_unit_t)pcnt_unit);
}
int16_t _esp32_readPulseCounter(uint8_t pcnt_unit) {
  // Serial.println(' ');
  // Serial.println(PCNT.cnt_unit[PCNT_UNIT_0].cnt_val);
  // Serial.println(PCNT.conf_unit[PCNT_UNIT_0].conf2.cnt_h_lim);
  return PCNT.cnt_unit[(pcnt_unit_t)pcnt_unit].cnt_val;
}
#endif
#endif
