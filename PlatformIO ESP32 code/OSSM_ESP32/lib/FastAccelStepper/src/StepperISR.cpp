#include <stdint.h>

#include "StepperISR.h"

int8_t StepperQueue::addQueueEntry(const struct stepper_command_s* cmd,
                                   bool start) {
  // Just to check if, if the struct has the correct size
  // if (sizeof(entry) != 6 * QUEUE_LEN) {
  //  return -1;
  //}
  if (!isReadyForCommands()) {
    return AQE_DEVICE_NOT_READY;
  }
  if (cmd == NULL) {
    if (start && !isRunning()) {
      if (next_write_idx == read_idx) {
        return AQE_ERROR_EMPTY_QUEUE_TO_START;
      }
      startQueue();
    }
    return AQE_OK;
  }
  if (isQueueFull()) {
    return AQE_QUEUE_FULL;
  }
  uint16_t period = cmd->ticks;
  uint8_t steps = cmd->steps;
  // Serial.print(period);
  // Serial.print(" ");
  // Serial.println(steps);

  uint32_t command_rate_ticks = period;
  if (steps > 1) {
    command_rate_ticks *= steps;
  }
  if (command_rate_ticks < MIN_CMD_TICKS) {
    return AQE_ERROR_TICKS_TOO_LOW;
  }

  uint8_t wp = next_write_idx;
  struct queue_entry* e = &entry[wp & QUEUE_LEN_MASK];
  bool dir = (cmd->count_up == dirHighCountsUp);
  bool toggle_dir = false;
#if defined(SUPPORT_EXTERNAL_DIRECTION_PIN)
  bool repeat_entry = false;
#endif
  if (dirPin != PIN_UNDEFINED) {
    if (isQueueEmpty() && ((dirPin & PIN_EXTERNAL_FLAG) == 0)) {
      // set the dirPin here. Necessary with shared direction pins
      digitalWrite(dirPin, dir);
#ifdef ARDUINO_ARCH_SAM
      delayMicroseconds(30);  // Make sure the driver has enough time to see
                              // the dir pin change
#endif
      queue_end.dir = dir;
    } else {
      toggle_dir = (dir != queue_end.dir);
#if defined(SUPPORT_EXTERNAL_DIRECTION_PIN)
      if (toggle_dir && (dirPin & PIN_EXTERNAL_FLAG)) {
        repeat_entry = toggle_dir;
        toggle_dir = false;
      }
#endif
    }
  }
  e->steps = steps;
#if defined(SUPPORT_EXTERNAL_DIRECTION_PIN)
  e->repeat_entry = repeat_entry;
  e->dirPinState = dir;
#endif
  e->toggle_dir = toggle_dir;
  e->countUp = cmd->count_up ? 1 : 0;
  e->moreThanOneStep = steps > 1 ? 1 : 0;
  e->hasSteps = steps > 0 ? 1 : 0;
  e->ticks = period;
  struct queue_end_s next_queue_end = queue_end;
#if defined(SUPPORT_QUEUE_ENTRY_START_POS_U16)
  e->start_pos_last16 = (uint32_t)next_queue_end.pos & 0xffff;
#endif
  next_queue_end.pos += cmd->count_up ? steps : -steps;
#if defined(SUPPORT_QUEUE_ENTRY_END_POS_U16)
  e->end_pos_last16 = (uint32_t)next_queue_end.pos & 0xffff;
#endif
  next_queue_end.dir = dir;
  next_queue_end.count_up = cmd->count_up;

  // Advance write pointer
  fasDisableInterrupts();
  if (!ignore_commands) {
    if (isReadyForCommands()) {
      next_write_idx++;
      queue_end = next_queue_end;
    } else {
      fasEnableInterrupts();
      return AQE_DEVICE_NOT_READY;
    }
  }
  fasEnableInterrupts();

  if (!isRunning() && start) {
    // stepper is not yet running and start is requested
    startQueue();
  }
  return AQE_OK;
}

int32_t StepperQueue::getCurrentPosition() {
  fasDisableInterrupts();
  uint32_t pos = (uint32_t)queue_end.pos;
  uint8_t rp = read_idx;
  bool is_empty = (rp == next_write_idx);
  struct queue_entry* e = &entry[rp & QUEUE_LEN_MASK];
#if defined(SUPPORT_QUEUE_ENTRY_END_POS_U16)
  uint16_t pos_last16 = e->end_pos_last16;
#endif
#if defined(SUPPORT_QUEUE_ENTRY_START_POS_U16)
  uint16_t pos_last16 = e->start_pos_last16;
#endif
  uint8_t steps = e->steps;
#if defined(SUPPORT_ESP32)
  // pulse counter should go max up to 255 with perhaps few pulses overrun, so
  // this conversion is safe
  int16_t done_p = (int16_t)_getPerformedPulses();
#endif
  fasEnableInterrupts();
#if defined(SUPPORT_ESP32)
  if (done_p == 0) {
    // fix for possible race condition described in issue #68
    fasDisableInterrupts();
    rp = read_idx;
    is_empty = (rp == next_write_idx);
    e = &entry[rp & QUEUE_LEN_MASK];
    pos_last16 = e->start_pos_last16;
    steps = e->steps;
    done_p = (int16_t)_getPerformedPulses();
    fasEnableInterrupts();
  }
#endif
  if (!is_empty) {
    int16_t adjust = 0;

    uint16_t pos16 = pos & 0xffff;
    uint8_t transition = ((pos16 >> 12) & 0x0c) | (pos_last16 >> 14);
    switch (transition) {
      case 0:   // 00 00
      case 5:   // 01 01
      case 10:  // 10 10
      case 15:  // 11 11
        break;
      case 1:   // 00 01
      case 6:   // 01 10
      case 11:  // 10 11
      case 12:  // 11 00
        pos += 0x4000;
        break;
      case 4:   // 01 00
      case 9:   // 10 01
      case 14:  // 11 10
      case 3:   // 00 11
        pos -= 0x4000;
        break;
      case 2:   // 00 10
      case 7:   // 01 11
      case 8:   // 10 00
      case 13:  // 11 01
        break;  // TODO: ERROR
    }
    pos = (int32_t)((pos & 0xffff0000) | pos_last16);

    if (steps != 0) {
      if (e->countUp) {
#if defined(SUPPORT_QUEUE_ENTRY_END_POS_U16)
        adjust = -steps;
#endif
#if defined(SUPPORT_QUEUE_ENTRY_START_POS_U16)
        adjust = done_p;
#endif
      } else {
#if defined(SUPPORT_QUEUE_ENTRY_END_POS_U16)
        adjust = steps;
#endif
#if defined(SUPPORT_QUEUE_ENTRY_START_POS_U16)
        adjust = -done_p;
#endif
      }
      pos += adjust;
    }
  }
  return pos;
}

uint32_t StepperQueue::ticksInQueue() {
  fasDisableInterrupts();
  uint8_t rp = read_idx;
  uint8_t wp = next_write_idx;
  fasEnableInterrupts();
  if (wp == rp) {
    return 0;
  }
  uint32_t ticks = 0;
  rp++;  // ignore currently processed entry
  while (wp != rp) {
    struct queue_entry* e = &entry[rp++ & QUEUE_LEN_MASK];
    ticks += e->ticks;
    uint8_t steps = e->steps;
    if (steps > 1) {
      uint32_t tmp = e->ticks;
      tmp *= steps - 1;
      ticks += tmp;
    }
  }
  return ticks;
}

bool StepperQueue::hasTicksInQueue(uint32_t min_ticks) {
  fasDisableInterrupts();
  uint8_t rp = read_idx;
  uint8_t wp = next_write_idx;
  fasEnableInterrupts();
  if (wp == rp) {
    return false;
  }
  rp++;  // ignore currently processed entry
  while (wp != rp) {
    struct queue_entry* e = &entry[rp & QUEUE_LEN_MASK];
    uint32_t tmp = e->ticks;
    uint8_t steps = max(e->steps, (uint8_t)1);
    tmp *= steps;
    if (tmp >= min_ticks) {
      return true;
    }
    min_ticks -= tmp;
    rp++;
  }
  return false;
}

uint16_t StepperQueue::getActualTicks() {
  // Retrieve current step rate from the current view.
  // This is valid only, if the command describes more than one step
  fasDisableInterrupts();
  uint8_t rp = read_idx;
  uint8_t wp = next_write_idx;
  fasEnableInterrupts();
  if (wp == rp) {
    return 0;
  }
  struct queue_entry* e = &entry[rp & QUEUE_LEN_MASK];
  if (e->hasSteps) {
    if (e->moreThanOneStep) {
      return e->ticks;
    }
    if (wp != ++rp) {
      if (entry[rp & QUEUE_LEN_MASK].hasSteps) {
        return e->ticks;
      }
    }
  }
  return 0;
}

void StepperQueue::_initVars() {
  dirPin = PIN_UNDEFINED;
#ifndef TEST
  max_speed_in_ticks = TICKS_PER_S / 1000;  // use a default value 1_000 steps/s
#else
  max_speed_in_ticks =
      TICKS_PER_S / 50000;  // use a default value 50_000 steps/s
#endif
  ignore_commands = false;
  read_idx = 0;
  next_write_idx = 0;
  queue_end.dir = true;
  queue_end.count_up = true;
  queue_end.pos = 0;
  dirHighCountsUp = true;
#if defined(ARDUINO_ARCH_AVR)
  _isRunning = false;
  _prepareForStop = false;
#endif
#if defined(SUPPORT_ESP32)
  _isRunning = false;
  _nextCommandIsPrepared = false;
#endif
#if defined(SUPPORT_ESP32_RMT)
  _rmtStopped = true;
#endif
#if defined(ARDUINO_ARCH_SAM)
  _hasISRactive = false;
  // we cannot clear the PWM interrupt when switching to a pause, but we'll
  // get a double interrupt if we do nothing.  So this tells us that on a
  // transition from a pulse to a pause to skip the next interrupt.
  _pauseCommanded = false;
  timePWMInterruptEnabled = 0;
#endif
#if defined(TEST)
  _isRunning = false;
#endif
}
