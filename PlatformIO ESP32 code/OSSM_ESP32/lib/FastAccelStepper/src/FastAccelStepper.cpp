#include "FastAccelStepper.h"
#include "StepperISR.h"

// This define in order to not shoot myself.
#ifndef TEST
#define printf DO_NOT_USE_PRINTF
#define puts DO_NOT_USE_PUTS
#endif

// Here are the global variables to interface with the interrupts

// To realize the 1 Hz debug led
static uint8_t fas_ledPin = PIN_UNDEFINED;
static uint16_t fas_debug_led_cnt = 0;

// dynamic allocation seems to not work so well on avr
FastAccelStepper fas_stepper[MAX_STEPPER];

//*************************************************************************************************
//*************************************************************************************************
void FastAccelStepperEngine::init() {
  _externalCallForPin = NULL;
  fas_init_engine(this, 255);
}

#if defined(SUPPORT_CPU_AFFINITY)
void FastAccelStepperEngine::init(uint8_t cpu_core) {
  _externalCallForPin = NULL;
  fas_init_engine(this, cpu_core);
}
#endif

//*************************************************************************************************
bool FastAccelStepperEngine::_isValidStepPin(uint8_t step_pin) {
  return StepperQueue::isValidStepPin(step_pin);
}
//*************************************************************************************************
bool FastAccelStepperEngine::isDirPinBusy(uint8_t dir_pin,
                                          uint8_t except_stepper) {
  for (uint8_t i = 0; i < MAX_STEPPER; i++) {
    if (i != except_stepper) {
      FastAccelStepper* s = _stepper[i];
      if (s) {
        if (s->getDirectionPin() == dir_pin) {
          if (s->isQueueRunning()) {
            return true;
          }
        }
      }
    }
  }
  return false;
}
//*************************************************************************************************
FastAccelStepper* FastAccelStepperEngine::stepperConnectToPin(
    uint8_t step_pin) {
  // Check if already connected
  for (uint8_t i = 0; i < MAX_STEPPER; i++) {
    FastAccelStepper* s = _stepper[i];
    if (s) {
      if (s->getStepPin() == step_pin) {
        return NULL;
      }
    }
  }
  if (!_isValidStepPin(step_pin)) {
    return NULL;
  }
  int8_t fas_stepper_num = StepperQueue::queueNumForStepPin(step_pin);
  if (fas_stepper_num < 0) {  // flexible, so just choose next
    if (_next_stepper_num >= MAX_STEPPER) {
      return NULL;
    }
    fas_stepper_num = _next_stepper_num;
  }
  uint8_t stepper_num = _next_stepper_num;
  _next_stepper_num++;

  FastAccelStepper* s = &fas_stepper[fas_stepper_num];
  _stepper[stepper_num] = s;
  s->init(this, fas_stepper_num, step_pin);
  for (uint8_t i = 0; i < _next_stepper_num; i++) {
    s = _stepper[i];
    fas_queue[s->_queue_num].adjustSpeedToStepperCount(_next_stepper_num);
  }
  return s;
}
//*************************************************************************************************
void FastAccelStepperEngine::setDebugLed(uint8_t ledPin) {
  fas_ledPin = ledPin;
  pinMode(fas_ledPin, OUTPUT);
  digitalWrite(fas_ledPin, LOW);
}
//*************************************************************************************************
void FastAccelStepperEngine::manageSteppers() {
#ifdef DEBUG_LED_HALF_PERIOD
  if (fas_ledPin != PIN_UNDEFINED) {
    fas_debug_led_cnt++;
    if (fas_debug_led_cnt == DEBUG_LED_HALF_PERIOD) {
      digitalWrite(fas_ledPin, HIGH);
    }
    if (fas_debug_led_cnt == 2 * DEBUG_LED_HALF_PERIOD) {
      digitalWrite(fas_ledPin, LOW);
      fas_debug_led_cnt = 0;
    }
  }
#endif
  for (uint8_t i = 0; i < _next_stepper_num; i++) {
    FastAccelStepper* s = _stepper[i];
    if (s) {
#ifdef SUPPORT_EXTERNAL_DIRECTION_PIN
      if (s->externalDirPinChangeCompletedIfNeeded()) {
        s->fill_queue();
      }
#else
      s->fill_queue();
#endif
    }
  }

  // Check for auto disable
  for (uint8_t i = 0; i < _next_stepper_num; i++) {
    FastAccelStepper* s = _stepper[i];
    if (s) {
      if (s->needAutoDisable()) {
        uint8_t high_active_pin = s->getEnablePinHighActive();
        uint8_t low_active_pin = s->getEnablePinLowActive();

        // fasDisableInterrupts(); // TODO
        bool agree = true;
        for (uint8_t j = 0; j < _next_stepper_num; j++) {
          if (i != j) {
            FastAccelStepper* other = _stepper[j];
            if (other) {
              if (other->usesAutoEnablePin(high_active_pin) ||
                  other->usesAutoEnablePin(low_active_pin)) {
                if (!other->agreeWithAutoDisable()) {
                  agree = false;
                  break;
                }
              }
            }
          }
        }
        if (agree) {
          for (uint8_t j = 0; j < _next_stepper_num; j++) {
            FastAccelStepper* current = _stepper[j];
            if (current) {
              if (current->usesAutoEnablePin(high_active_pin) ||
                  current->usesAutoEnablePin(low_active_pin)) {
                // if successful, then the _auto_disable_delay_counter is zero
                // Otherwise in next loop will be checked for auto disable again
                current->disableOutputs();
              }
            }
          }
        }
        // fasEnableInterrupts();
      }
    }
  }

  // Update the auto disable counters
  for (uint8_t i = 0; i < _next_stepper_num; i++) {
    FastAccelStepper* s = _stepper[i];
    if (s) {
      fasDisableInterrupts();
      // update the counters down to 1
      s->updateAutoDisable();
      fasEnableInterrupts();
    }
  }
}

//*************************************************************************************************
//*************************************************************************************************
//
// FastAccelStepper provides:
// - movement control
//       either raw access to the stepper command queue
//       or ramp generator driven by speed/acceleration and move
// - stepper position
//
// This implements auto enable and delay from direction change to first step
//
//*************************************************************************************************
//*************************************************************************************************

//*************************************************************************************************
int8_t FastAccelStepper::addQueueEntry(const struct stepper_command_s* cmd,
                                       bool start) {
  StepperQueue* q = &fas_queue[_queue_num];
  if (cmd == NULL) {
    return q->addQueueEntry(NULL, start);
  }
  if (cmd->ticks < q->max_speed_in_ticks) {
    return AQE_ERROR_TICKS_TOO_LOW;
  }

  if (_dirPin != PIN_UNDEFINED) {
    if (!isQueueRunning()) {
      if (_engine != NULL) {
        if (_engine->isDirPinBusy(_dirPin, _queue_num)) {
          return AQE_DIR_PIN_IS_BUSY;
        }
      }
    }
  } else {
    if (!cmd->count_up) {
      return AQE_ERROR_NO_DIR_PIN_TO_TOGGLE;
    }
  }

  int res = AQE_OK;
  if (_autoEnable) {
    fasDisableInterrupts();
    uint16_t delay_counter = _auto_disable_delay_counter;
    fasEnableInterrupts();
    if (delay_counter == 0) {
      // outputs are disabled
      if (!enableOutputs()) {
        return AQE_WAIT_FOR_ENABLE_PIN_ACTIVE;
      }
      // if on delay is defined, fill queue if required amount of pauses before
      // the first step
      if (_on_delay_ticks > 0) {
        uint32_t delay = _on_delay_ticks;
        // this delay sets count_up appropriately. If this is shorter than
        // dir_change_delay_ticks, then extend accordingly
        if ((delay < _dir_change_delay_ticks) &&
            (q->queue_end.count_up != cmd->count_up)) {
          delay = _dir_change_delay_ticks;
        }
        while (delay > 0) {
          uint32_t ticks = delay >> 1;
          uint16_t ticks_u16 = ticks;
          if (ticks > 65535) {
            ticks_u16 = 65535;
          } else if (ticks < 32768) {
            ticks_u16 = delay;
          }
          struct stepper_command_s start_cmd = {
              .ticks = ticks_u16, .steps = 0, .count_up = cmd->count_up};
          q->addQueueEntry(&start_cmd, false);
          delay -= ticks_u16;
        }
        res = q->addQueueEntry(NULL, start);
        if (res != AQE_OK) {
          return res;
        }
      }
    }
  }
  if (q->queue_end.count_up != cmd->count_up) {
    // Change of direction has been detected.
    if (_dirPin & PIN_EXTERNAL_FLAG) {
      // for external pins, two pause commands need to be added. The first one
      // with the dir pin change. The second one just a pause.
      // The queue's addQueueEntry() will set repeat_entry for the command entry
      if (q->queueEntries() > QUEUE_LEN - 2) {
        // no space for two commands => do nothing and return QUEUE_FULL
        return AQE_QUEUE_FULL;
      }
      struct stepper_command_s start_cmd = {
          .ticks = US_TO_TICKS(500), .steps = 0, .count_up = cmd->count_up};
      res = q->addQueueEntry(&start_cmd, start);
      if (res != AQE_OK) {
        return res;
      }
      res = q->addQueueEntry(&start_cmd, start);
      if (res != AQE_OK) {
        return res;
      }
    } else if ((_dir_change_delay_ticks != 0) && (cmd->steps != 0)) {
      // add pause command to delay dir pin change to first step
      struct stepper_command_s start_cmd = {.ticks = _dir_change_delay_ticks,
                                            .steps = 0,
                                            .count_up = cmd->count_up};
      res = q->addQueueEntry(&start_cmd, start);
      if (res != AQE_OK) {
        return res;
      }
    }
  }
  res = q->addQueueEntry(cmd, start);
  if (_autoEnable) {
    if (res == AQE_OK) {
      fasDisableInterrupts();
      _auto_disable_delay_counter = _off_delay_count;
      fasEnableInterrupts();
    }
  }

  return res;
}

#ifdef SUPPORT_EXTERNAL_DIRECTION_PIN
bool FastAccelStepper::externalDirPinChangeCompletedIfNeeded() {
  StepperQueue* q = &fas_queue[_queue_num];
  if ((_dirPin != PIN_UNDEFINED) && (_dirPin & PIN_EXTERNAL_FLAG)) {
    if (q->isOnRepeatingEntry()) {
      if (_engine->_externalCallForPin) {
        uint8_t state = q->dirPinState();
        bool newState = _engine->_externalCallForPin(_dirPin, state);
        if (newState != state) {
          return false;
        }
        q->clearRepeatingFlag();
      }
    }
  }
  return true;
}
#endif

//*************************************************************************************************
// fill_queue generates commands to the stepper for executing a ramp
//
// Plan is to fill the queue with commmands summing up to approx. 10 ms in the
// future (or more). For low speeds, this results in single stepping For high
// speeds (40kSteps/s) approx. 400 Steps to be created using 3 commands
//
//*************************************************************************************************

void FastAccelStepper::fill_queue() {
  // Check preconditions to be allowed to fill the queue
  if (!_rg.isRampGeneratorActive()) {
    return;
  }
  if (!_rg.hasValidConfig()) {
#ifdef TEST
    assert(false);
#endif
    return;
  }
  // check if addition of commands is suspended (due to forceStopAndNewPosition)
  StepperQueue* q = &fas_queue[_queue_num];

  // if force stop has been called, then ignore_commands is true and ramp
  // stopped. So the ramp generator will not create a new command, unless new
  // move command has been given after forceStop..(). So we just clear the flag
  q->ignore_commands = false;

  // preconditions are fulfilled, so create the command(s)
  NextCommand cmd;
  // Plan ahead for max. 20 ms. Currently hard coded
  bool delayed_start = !q->isRunning();
  bool need_delayed_start = false;
  uint32_t ticksPrepared = q->ticksInQueue();
  while (!isQueueFull() &&
         ((ticksPrepared < TICKS_PER_S / 50) || q->queueEntries() <= 1) &&
         _rg.isRampGeneratorActive()) {
#if (TEST_MEASURE_ISR_SINGLE_FILL == 1)
    // For run time measurement
    uint32_t runtime_us = micros();
#endif
    int8_t res = AQE_OK;
    _rg.getNextCommand(&q->queue_end, &cmd);
    if (cmd.command.ticks != 0) {
      res = addQueueEntry(&cmd.command, !delayed_start);
    }
    if (res == AQE_OK) {
      _rg.afterCommandEnqueued(&cmd);
      need_delayed_start = delayed_start;
      if (cmd.command.steps <= 1) {
        ticksPrepared += cmd.command.ticks;
      } else {
        uint32_t tmp = cmd.command.ticks;
        tmp *= cmd.command.steps;
        ticksPrepared += tmp;
      }
    }

#if (TEST_MEASURE_ISR_SINGLE_FILL == 1)
    // For run time measurement
    runtime_us = micros() - runtime_us;
    max_micros = max(max_micros, runtime_us);
#endif
    if (cmd.command.ticks == 0) {
      break;
    }
    if (res != AQE_OK) {
      if (res > 0) {
        // try later again
        break;
      } else {
#ifdef SIM_TEST_INPUT
        Serial.println("Abort ramp due to queue error");
        Serial.print("Steps=");
        Serial.print(cmd.command.steps);
        Serial.print(" ticks=");
        Serial.print(cmd.command.ticks);
        Serial.print(" min_cmd_ticks=");
        Serial.println(MIN_CMD_TICKS);
#endif
#ifdef TEST
        printf("ERROR: Abort ramp due to queue error (%d)\n", res);
        printf("steps=%d ticks=%d limit=%ld state=%d\n", cmd.command.steps,
               cmd.command.ticks, MIN_CMD_TICKS, cmd.rw.ramp_state);
        assert(false);
#endif
        _rg.stopRamp();
        delayed_start = false;
      }
    }
  }
  if (need_delayed_start) {
    addQueueEntry(NULL, true);
  }
}

void FastAccelStepper::updateAutoDisable() {
  // FastAccelStepperEngine will call with interrupts disabled
  // fasDisableInterrupts();
  if (_auto_disable_delay_counter > 1) {
    if (!isRunning()) {
      _auto_disable_delay_counter--;
    }
  }
  // fasEnableInterrupts();
}

bool FastAccelStepper::agreeWithAutoDisable() {
  bool agree = true;
  // FastAccelStepperEngine will call with interrupts disabled
  // fasDisableInterrupts();
  if (isRunning()) {
    agree = false;
  }
  if (_auto_disable_delay_counter > 1) {
    agree = false;
  }
  // fasEnableInterrupts();
  return agree;
}

bool FastAccelStepper::needAutoDisable() {
  bool need_disable = false;
  // FastAccelStepperEngine will call with interrupts disabled
  // fasDisableInterrupts();
  if (_auto_disable_delay_counter == 1) {
    if (!isRunning()) {
      need_disable = true;
    }
  }
  // fasEnableInterrupts();
  return need_disable;
}

bool FastAccelStepper::usesAutoEnablePin(uint8_t pin) {
  if (pin != PIN_UNDEFINED) {
    if ((pin == _enablePinHighActive) || (pin == _enablePinLowActive)) {
      return true;
    }
  }
  return false;
}

void FastAccelStepper::init(FastAccelStepperEngine* engine, uint8_t num,
                            uint8_t step_pin) {
#if (TEST_MEASURE_ISR_SINGLE_FILL == 1)
  // For run time measurement
  max_micros = 0;
#endif
  _engine = engine;
  _autoEnable = false;
  _dir_change_delay_ticks = 0;
  _on_delay_ticks = 0;
  _off_delay_count = 1;
  _auto_disable_delay_counter = 0;
  _stepPin = step_pin;
  _dirHighCountsUp = true;
  _dirPin = PIN_UNDEFINED;
  _rg.init();

  _queue_num = num;
  fas_queue[_queue_num].init(_queue_num, step_pin);
#if defined(SUPPORT_ESP32_PULSE_COUNTER)
  _attached_pulse_cnt_unit = -1;
#endif
}
uint8_t FastAccelStepper::getStepPin() { return _stepPin; }
void FastAccelStepper::setDirectionPin(uint8_t dirPin, bool dirHighCountsUp,
                                       uint16_t dir_change_delay_us) {
  _dirPin = dirPin;
  _dirHighCountsUp = dirHighCountsUp;
  if (_dirPin != PIN_UNDEFINED) {
    if (_dirPin & PIN_EXTERNAL_FLAG) {
      if (_engine->_externalCallForPin) {
        _engine->_externalCallForPin(_dirPin, dirHighCountsUp ? HIGH : LOW);
      }
    } else {
      digitalWrite(dirPin, dirHighCountsUp ? HIGH : LOW);
      pinMode(dirPin, OUTPUT);
    }
  }
  fas_queue[_queue_num].setDirPin(dirPin, dirHighCountsUp);
  if (dir_change_delay_us != 0) {
    if (dir_change_delay_us > MAX_DIR_DELAY_US) {
      dir_change_delay_us = MAX_DIR_DELAY_US;
    }
    if (dir_change_delay_us < MIN_DIR_DELAY_US) {
      dir_change_delay_us = MIN_DIR_DELAY_US;
    }
    _dir_change_delay_ticks = US_TO_TICKS(dir_change_delay_us);
  } else {
    _dir_change_delay_ticks = 0;
  }
}
void FastAccelStepper::setEnablePin(uint8_t enablePin,
                                    bool low_active_enables_stepper) {
  if (low_active_enables_stepper) {
    _enablePinLowActive = enablePin;
    if (enablePin != PIN_UNDEFINED) {
      if (enablePin & PIN_EXTERNAL_FLAG) {
        if (_engine->_externalCallForPin) {
          _engine->_externalCallForPin(enablePin, HIGH);
        }
      } else {
        digitalWrite(enablePin, HIGH);
        pinMode(enablePin, OUTPUT);
        if (_enablePinHighActive == enablePin) {
          _enablePinHighActive = PIN_UNDEFINED;
        }
      }
    }
  } else {
    _enablePinHighActive = enablePin;
    if (enablePin != PIN_UNDEFINED) {
      if (enablePin & PIN_EXTERNAL_FLAG) {
        if (_engine->_externalCallForPin) {
          _engine->_externalCallForPin(enablePin, LOW);
        }
      } else {
        digitalWrite(enablePin, LOW);
        pinMode(enablePin, OUTPUT);
        if (_enablePinLowActive == enablePin) {
          _enablePinLowActive = PIN_UNDEFINED;
        }
      }
    }
  }
}
void FastAccelStepper::setAutoEnable(bool auto_enable) {
  _autoEnable = auto_enable;
  if (auto_enable && (_off_delay_count == 0)) {
    _off_delay_count = 1;
  }
}
int8_t FastAccelStepper::setDelayToEnable(uint32_t delay_us) {
  uint32_t delay_ticks = US_TO_TICKS(delay_us);
  if (delay_ticks > 0) {
    if (delay_ticks < MIN_CMD_TICKS) {
      return DELAY_TOO_LOW;
    }
  }
  if (delay_ticks > MAX_ON_DELAY_TICKS) {
    return DELAY_TOO_HIGH;
  }
  _on_delay_ticks = delay_ticks;
  return DELAY_OK;
}
void FastAccelStepper::setDelayToDisable(uint16_t delay_ms) {
  uint16_t delay_count = delay_ms / DELAY_MS_BASE;
  if ((delay_ms > 0) && (delay_count < 2)) {
    // ensure minimum time
    delay_count = 2;
  }
  _off_delay_count = max(delay_count, (uint16_t)1);
}
int8_t FastAccelStepper::runForward() { return _rg.startRun(true); }
int8_t FastAccelStepper::runBackward() { return _rg.startRun(false); }
int8_t FastAccelStepper::moveTo(int32_t position, bool blocking) {
  int8_t res = _rg.moveTo(position, &fas_queue[_queue_num].queue_end);
  if ((res == MOVE_OK) && blocking) {
    while (isRunning()) {
      noop_or_wait;
    }
  }
  return res;
}
int8_t FastAccelStepper::move(int32_t move, bool blocking) {
  if ((move < 0) && (_dirPin == PIN_UNDEFINED)) {
    return MOVE_ERR_NO_DIRECTION_PIN;
  }
  int8_t res = _rg.move(move, &fas_queue[_queue_num].queue_end);
  if ((res == MOVE_OK) && blocking) {
    while (isRunning()) {
      noop_or_wait;
    }
  }
  return res;
}
void FastAccelStepper::keepRunning() { _rg.setKeepRunning(); }
void FastAccelStepper::stopMove() { _rg.initiateStop(); }
void FastAccelStepper::applySpeedAcceleration() {
  _rg.applySpeedAcceleration();
}
int8_t FastAccelStepper::moveByAcceleration(int32_t acceleration,
                                            bool allow_reverse) {
  int8_t res = MOVE_OK;
  if (acceleration > 0) {
    setAcceleration(acceleration);
    res = runForward();
  } else if (acceleration < 0) {
    setAcceleration(-acceleration);
    if (allow_reverse && (_dirPin != PIN_UNDEFINED)) {
      res = runBackward();
    } else {
      applySpeedAcceleration();
      stopMove();
    }
  } else {
    uint32_t max_speed = _rg.getSpeedInTicks();
    setSpeedInTicks(getPeriodInTicksAfterCommandsCompleted());
    setAcceleration(1);  // ensure increase, so the speed is kept
    applySpeedAcceleration();
    setSpeedInTicks(max_speed);
  }
  return res;
}
void FastAccelStepper::forceStop() {
  StepperQueue* q = &fas_queue[_queue_num];

  // ensure no more commands are added to the queue
  q->ignore_commands = true;

  // inform ramp generator to force stop
  _rg.forceStop();
}
void FastAccelStepper::forceStopAndNewPosition(uint32_t new_pos) {
  StepperQueue* q = &fas_queue[_queue_num];

  // ensure no more commands are added to the queue
  q->ignore_commands = true;

  // inform ramp generator to force stop
  _rg.forceStop();

  // stop the stepper interrupt and empty the queue
  q->forceStop();

  // set the new position. This is not safe
  q->queue_end.pos = new_pos;
}
bool FastAccelStepper::disableOutputs() {
  if (isRunning() && _autoEnable) {
    return false;
  }
  bool disabled = true;
  if (_enablePinLowActive != PIN_UNDEFINED) {
    if (_enablePinLowActive & PIN_EXTERNAL_FLAG) {
      if (_engine->_externalCallForPin != NULL) {
        disabled &=
            (_engine->_externalCallForPin(_enablePinLowActive, HIGH) == HIGH);
      }
    } else {
      digitalWrite(_enablePinLowActive, HIGH);
    }
  }
  if (_enablePinHighActive != PIN_UNDEFINED) {
    if (_enablePinHighActive & PIN_EXTERNAL_FLAG) {
      if (_engine->_externalCallForPin != NULL) {
        disabled &=
            (_engine->_externalCallForPin(_enablePinHighActive, LOW) == LOW);
      }
    } else {
      digitalWrite(_enablePinHighActive, LOW);
    }
  }
  if (disabled) {
    _auto_disable_delay_counter = 0;
  }
  return disabled;
}
bool FastAccelStepper::enableOutputs() {
  bool enabled = true;
  if (_enablePinLowActive != PIN_UNDEFINED) {
    if (_enablePinLowActive & PIN_EXTERNAL_FLAG) {
      if (_engine->_externalCallForPin != NULL) {
        enabled &=
            (_engine->_externalCallForPin(_enablePinLowActive, LOW) == LOW);
      }
    } else {
      digitalWrite(_enablePinLowActive, LOW);
    }
  }
  if (_enablePinHighActive != PIN_UNDEFINED) {
    if (_enablePinHighActive & PIN_EXTERNAL_FLAG) {
      if (_engine->_externalCallForPin != NULL) {
        enabled &=
            (_engine->_externalCallForPin(_enablePinHighActive, HIGH) == HIGH);
      }
    } else {
      digitalWrite(_enablePinHighActive, HIGH);
    }
  }
  return enabled;
}
int32_t FastAccelStepper::getPositionAfterCommandsCompleted() {
  return fas_queue[_queue_num].queue_end.pos;
}
uint32_t FastAccelStepper::getPeriodInTicksAfterCommandsCompleted() {
  if (_rg.isRampGeneratorActive()) {
    return _rg.getCurrentPeriodInTicks();
  }
  return 0;
}
uint32_t FastAccelStepper::getPeriodInUsAfterCommandsCompleted() {
  if (_rg.isRampGeneratorActive()) {
    return _rg.getCurrentPeriodInUs();
  }
  return 0;
}
int32_t FastAccelStepper::getCurrentSpeedInUs() {
  uint32_t ticks = fas_queue[_queue_num].getActualTicks();
  if (ticks == 0) {
    ticks = getPeriodInTicksAfterCommandsCompleted();
  }
  int32_t speed_in_us = ticks / (TICKS_PER_S / 1000000);
  switch (_rg.rampState() & RAMP_DIRECTION_MASK) {
    case RAMP_DIRECTION_COUNT_UP:
      return speed_in_us;
    case RAMP_DIRECTION_COUNT_DOWN:
      return -speed_in_us;
  }
  return 0;
}
int32_t FastAccelStepper::getCurrentSpeedInMilliHz() {
  uint32_t ticks = fas_queue[_queue_num].getActualTicks();
  if (ticks == 0) {
    ticks = getPeriodInTicksAfterCommandsCompleted();
  }
  int32_t speed_in_milli_hz = 0;
  if (ticks > 0) {
    speed_in_milli_hz = ((uint32_t)250 * TICKS_PER_S) / ticks * 4;
  }
  switch (_rg.rampState() & RAMP_DIRECTION_MASK) {
    case RAMP_DIRECTION_COUNT_UP:
      return speed_in_milli_hz;
    case RAMP_DIRECTION_COUNT_DOWN:
      return -speed_in_milli_hz;
  }
  return 0;
}
uint16_t FastAccelStepper::getMaxSpeedInTicks() {
  return fas_queue[_queue_num].getMaxSpeedInTicks();
}
uint16_t FastAccelStepper::getMaxSpeedInUs() {
  uint16_t ticks = getMaxSpeedInTicks();
  uint16_t speed_in_us = ticks / (TICKS_PER_S / 1000000);
  return speed_in_us;
}
uint32_t FastAccelStepper::getMaxSpeedInHz() {
  uint16_t ticks = getMaxSpeedInTicks();
  uint32_t speed_in_hz = TICKS_PER_S / ticks;
  return speed_in_hz;
}
uint32_t FastAccelStepper::getMaxSpeedInMilliHz() {
  uint16_t ticks = getMaxSpeedInTicks();
  uint32_t speed_in_milli_hz = ((uint32_t)250 * TICKS_PER_S) / ticks * 4;
  return speed_in_milli_hz;
}
int8_t FastAccelStepper::setSpeedInTicks(uint32_t min_step_ticks) {
  if (min_step_ticks < getMaxSpeedInTicks()) {
    return -1;
  }
  if (min_step_ticks == TICKS_FOR_STOPPED_MOTOR) {
    return -1;
  }
  _rg.setSpeedInTicks(min_step_ticks);
  return 0;
}
int8_t FastAccelStepper::setSpeedInUs(uint32_t min_step_us) {
  if (min_step_us >= TICKS_TO_US(0xffffffff)) {
    return -1;
  }
  uint32_t min_step_ticks = US_TO_TICKS(min_step_us);
  return setSpeedInTicks(min_step_ticks);
}
int8_t FastAccelStepper::setSpeedInHz(uint32_t speed_hz) {
  if (speed_hz == 0) {
    return -1;
  }
  uint32_t ticks = _rg.divForHz(speed_hz);
  return setSpeedInTicks(ticks);
}
int8_t FastAccelStepper::setSpeedInMilliHz(uint32_t speed_mhz) {
  if (speed_mhz <= (1000LL * TICKS_PER_S / 0xffffffff + 1)) {
    return -1;
  }
  uint32_t ticks = _rg.divForMilliHz(speed_mhz);
  return setSpeedInTicks(ticks);
}
void FastAccelStepper::setCurrentPosition(int32_t new_pos) {
  int32_t delta = new_pos - getCurrentPosition();
  if (delta != 0) {
    struct queue_end_s* queue_end = &fas_queue[_queue_num].queue_end;
    fasDisableInterrupts();
    queue_end->pos += delta;
    fasEnableInterrupts();
    _rg.advanceTargetPosition(delta, queue_end);
  }
}
void FastAccelStepper::setPositionAfterCommandsCompleted(int32_t new_pos) {
  struct queue_end_s* queue_end = &fas_queue[_queue_num].queue_end;
  fasDisableInterrupts();
  int32_t delta = new_pos - fas_queue[_queue_num].queue_end.pos;
  queue_end->pos = new_pos;
  fasEnableInterrupts();
  if (delta != 0) {
    _rg.advanceTargetPosition(delta, queue_end);
  }
}
uint8_t FastAccelStepper::queueEntries() {
  return fas_queue[_queue_num].queueEntries();
}
uint32_t FastAccelStepper::ticksInQueue() {
  return fas_queue[_queue_num].ticksInQueue();
}
bool FastAccelStepper::hasTicksInQueue(uint32_t min_ticks) {
  return fas_queue[_queue_num].hasTicksInQueue(min_ticks);
}
bool FastAccelStepper::isQueueFull() {
  return fas_queue[_queue_num].isQueueFull();
}
bool FastAccelStepper::isQueueEmpty() {
  return fas_queue[_queue_num].isQueueEmpty();
}
bool FastAccelStepper::isQueueRunning() {
  return fas_queue[_queue_num].isRunning();
}
bool FastAccelStepper::isRunning() {
  return fas_queue[_queue_num].isRunning() || _rg.isRampGeneratorActive();
}
void FastAccelStepper::performOneStep(bool count_up, bool blocking) {
  if (!isRunning()) {
    if (count_up || (_dirPin != PIN_UNDEFINED)) {
      struct stepper_command_s cmd = {
          .ticks = MIN_CMD_TICKS, .steps = 1, .count_up = count_up};
      addQueueEntry(&cmd);
      if (blocking) {
        while (isRunning()) {
        }
      }
    }
  }
}
void FastAccelStepper::forwardStep(bool blocking) {
  performOneStep(true, blocking);
}
void FastAccelStepper::backwardStep(bool blocking) {
  performOneStep(false, blocking);
}
int32_t FastAccelStepper::getCurrentPosition() {
  return fas_queue[_queue_num].getCurrentPosition();
}
void FastAccelStepper::detachFromPin() { fas_queue[_queue_num].disconnect(); }
void FastAccelStepper::reAttachToPin() { fas_queue[_queue_num].connect(); }
#if defined(SUPPORT_ESP32_PULSE_COUNTER)
bool FastAccelStepper::attachToPulseCounter(uint8_t pcnt_unit,
                                            int16_t low_value,
                                            int16_t high_value) {
  if (pcnt_unit < 8) {
    if (_esp32_attachToPulseCounter(pcnt_unit, this, low_value, high_value)) {
      _attached_pulse_cnt_unit = pcnt_unit;
      return true;
    }
  }
  return false;
}
void FastAccelStepper::clearPulseCounter() {
  if (_attached_pulse_cnt_unit >= 0) {
    _esp32_clearPulseCounter(_attached_pulse_cnt_unit);
  }
}
int16_t FastAccelStepper::readPulseCounter() {
  if (_attached_pulse_cnt_unit >= 0) {
    return _esp32_readPulseCounter(_attached_pulse_cnt_unit);
  }
  return 0;
}
#endif
