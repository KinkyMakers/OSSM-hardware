#ifndef RAMP_GENERATOR_H
#define RAMP_GENERATOR_H

#include "FastAccelStepper.h"
#include "RampCalculator.h"
#include "RampConstAcceleration.h"
#include "common.h"

class FastAccelStepper;

#ifdef SUPPORT_UPM_TIMER_FREQ_VARIABLES
extern upm_float upm_timer_freq;
extern upm_float upm_timer_freq_div_500;
extern upm_float upm_timer_freq_div_sqrt_of_2;
extern upm_float upm_timer_freq_square_div_2;
#endif

class RampGenerator {
 private:
  // Latest configuration for acceleration/speed calculate_move, only
  struct ramp_config_s _config;

  // The ro variables are those, which are only read from _getNextCommand().
  // The rw variables are only read and written by _getNextCommand() and
  // commandEnqueued() Reading ro variables is safe in application
  struct ramp_ro_s _ro;
  struct ramp_rw_s _rw;

 public:
  uint32_t speed_in_ticks;
  uint32_t acceleration;
  inline uint8_t rampState() { return _rw.rampState(); }
  void init();
  inline int32_t targetPosition() { return _ro.targetPosition(); }
  void advanceTargetPosition(int32_t delta, const struct queue_end_s *queue);
  void setSpeedInTicks(uint32_t min_step_ticks);
  uint32_t getSpeedInUs() { return speed_in_ticks / (TICKS_PER_S / 1000000); }
  uint32_t getSpeedInTicks() { return speed_in_ticks; }
  uint32_t divForMilliHz(uint32_t f) {
    uint32_t base = (uint32_t)250 * TICKS_PER_S;
    uint32_t res = base / f;
    base -= res * f;
    base <<= 2;
    res <<= 2;
    base += f / 2;  // add rounding
    res += base / f;
    return res;
  }
  uint32_t divForHz(uint32_t f) {
    uint32_t base = TICKS_PER_S;
    base += f / 2;  // add rounding
    uint32_t res = base / f;
    return res;
  }
  uint32_t getSpeedInMilliHz() {
    if (speed_in_ticks == 0) {
      return 0;
    }
    return divForMilliHz(speed_in_ticks);
  }
  int8_t setAcceleration(int32_t accel);
  uint32_t getAcceleration() { return acceleration; }
  int32_t getCurrentAcceleration();
  inline bool hasValidConfig() { return _config.checkValidConfig() == MOVE_OK; }
  void applySpeedAcceleration();
  int8_t move(int32_t move, const struct queue_end_s *queue);
  int8_t moveTo(int32_t position, const struct queue_end_s *queue);
  int8_t startRun(bool countUp);
  inline void forceStop() { _ro.immediateStop(); }
  inline void initiateStop() { _ro.initiateStop(); }
  inline bool isStopping() {
    return _ro.isStopInitiated() && isRampGeneratorActive();
  }
  bool isRampGeneratorActive() { return rampState() != RAMP_STATE_IDLE; }

  void stopRamp();
  inline void setKeepRunning() { _ro.setKeepRunning(); }
  inline bool isRunningContinuously() { return _ro.isRunningContinuously(); }
  void getNextCommand(const struct queue_end_s *queue_end,
                      NextCommand *cmd_out);
  void afterCommandEnqueued(NextCommand *cmd_in);
  uint32_t getCurrentPeriodInTicks() {
    fasDisableInterrupts();
    uint32_t ticks = _rw.curr_ticks;
    fasEnableInterrupts();
    return ticks;
  }
  uint32_t getCurrentPeriodInUs() {
    fasDisableInterrupts();
    uint32_t ticks = _rw.curr_ticks;
    fasEnableInterrupts();
    return TICKS_TO_US(ticks);
  }

 private:
  int8_t _startMove(int32_t target_pos, bool position_changed);
};

#endif
