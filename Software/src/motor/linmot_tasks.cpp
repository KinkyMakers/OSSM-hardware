#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "CO_main.h"

#include "esp_log.h"
#include "data_logger.hpp"

#include "config.h"

#include "motor/linmot.hpp"

//#include "blynk.hpp"

void LinmotMotor::task_motion() {
  OD_get_u16(this->CO_statusWord_entry, 0x01, &this->CO_statusWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x02, &this->CO_runWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x03, &this->CO_errorWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x04, &this->CO_warnWord, false);

  uint16_t statusWord = this->CO_statusWord;
  uint16_t errorWord = this->CO_errorWord;

  bool isEnabled = (statusWord & LINMOT_STATUS_OPERATION_ENABLED) > 0;
  // bool isActive = (statusWord & LINMOT_STATUS_SWITCH_ON) > 0;
  bool isHomed = (statusWord & LINMOT_STATUS_HOMED) > 0;
  bool isMoving = (statusWord & LINMOT_STATUS_MOTION_ACTIVE) > 0;
  bool isError = (statusWord & LINMOT_STATUS_ERROR) > 0;

  bool isCanTimeout = (errorWord == LINMOT_ERROR_CANBUS_GUARD_TIMEOUT);
  bool isMotionWrong = (errorWord == LINMOT_ERROR_MOTION_CMD_WRONG_STATE);
  if (isError) {
    if (isCanTimeout || isMotionWrong) {
      WEB_LOGI("task.main", "LinMot is in a known error state. Acknowledging! Timeout %d - Motion %d", isCanTimeout, isMotionWrong);

      this->CO_control_addFlag(LINMOT_CONTROL_ERROR_ACKNOWLEDGE);
      vTaskDelay(50 / portTICK_PERIOD_MS);

      this->CO_control_removeFlag(LINMOT_CONTROL_ERROR_ACKNOWLEDGE);
      vTaskDelay(50 / portTICK_PERIOD_MS);

      return;
    } else {
      WEB_LOGE("task.main", "LinMot is in a unknown error state 0x%04X! Unrecoverable, attempting to restart drive!", errorWord);
      CO_NMT_sendCommand(CO->NMT, CO_NMT_RESET_NODE, this->CO_nodeId);

      // TODO - Bit of a hack to make sure error waits until refresh
      OD_set_u16(this->CO_statusWord_entry, 0x01, 0x0000, false);
      OD_set_u16(this->CO_statusWord_entry, 0x02, 0x0000, false);
      OD_set_u16(this->CO_statusWord_entry, 0x03, 0x0000, false);
      OD_set_u16(this->CO_statusWord_entry, 0x04, 0x0000, false);

      vTaskDelay(5000 / portTICK_PERIOD_MS);
      return;
    }
  }

  if (!isEnabled) {
    WEB_LOGI("task.main", "Switching on LinMot!");

    this->CO_control_removeFlag(LINMOT_CONTROL_SWITCH_ON);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    this->CO_control_addFlag(LINMOT_CONTROL_SWITCH_ON);
    vTaskDelay(250 / portTICK_PERIOD_MS);

    return;
  }

  if (isEnabled && !isHomed && !isMoving) {
    WEB_LOGI("task.main", "Homing LinMot!");
    // TODO - Should shut off homing state after this
    // This is why Motion CMD error is encountered after rebooting drive!

    // TODO - This is a hack to fix motion cmd error
    this->state = MotorState::HOMING;
    this->CO_control_addFlag(LINMOT_CONTROL_HOME);

    vTaskDelay(5000 / portTICK_PERIOD_MS);
    this->CO_control_removeFlag(LINMOT_CONTROL_HOME);
    vTaskDelay(50 / portTICK_PERIOD_MS);

    this->state = MotorState::ACTIVE;

    return;
  }
}

void LinmotMotor::task_heartbeat() {
  if (this->hasInitialized == false) {
    /*Set Operating Mode of Slaves to Operational*/
    WEB_LOGI("task.main", "Setting LinMot to Operational State!");
    CO_NMT_sendCommand(CO->NMT, CO_NMT_ENTER_OPERATIONAL, this->CO_nodeId);
    this->hasInitialized = true;
  }

  // It's been 1000ms since we've last gotten a LinMot Status update
  time_t now;
  time(&now);

  double diff = difftime(now, this->lastRPDOUpdate);
  if (diff > 1.0) {
    WEB_LOGE("task.main", "Error: Have not recieved LinMot RPDO in %d ms! Attempting to re-establish!", (int)(diff * 1000));
    this->state = MotorState::ERROR;
    CO_NMT_sendCommand(CO->NMT, CO_NMT_ENTER_OPERATIONAL, this->CO_nodeId);
  }

  uint8_t runState = (this->CO_runWord & 0xff00) >> 8;
  // TODO - Build callback so Blynk can be moved to CANFuck
  /*
  Blynk.virtualWrite(BLYNK_LINMOT_STATUS_PIN, (float)this->CO_statusWord);
  Blynk.virtualWrite(BLYNK_LINMOT_STATE_PIN, (float)runState);
  Blynk.virtualWrite(BLYNK_STATUS_PIN, (float)this->status);
  Blynk.virtualWrite(BLYNK_STATE_PIN, this->getStateString());
  
  Blynk.virtualWrite(BLYNK_LINMOT_ACTUAL_POSITION, (float)this->CO_actualPositionWord);
  Blynk.virtualWrite(BLYNK_LINMOT_DEMAND_CURRENT, (float)this->CO_demandCurrentWord);
  Blynk.virtualWrite(BLYNK_LINMOT_DEMAND_POSITION, (float)this->CO_demandPositionWord);

  Blynk.virtualWrite(BLYNK_LINMOT_MODEL_TEMP, (float)this->CO_modelTempWord);
  Blynk.virtualWrite(BLYNK_LINMOT_REAL_TEMP, (float)this->CO_realTempWord);
  Blynk.virtualWrite(BLYNK_LINMOT_MOTOR_VOLTAGE, (float)this->CO_motorVoltageWord);
  Blynk.virtualWrite(BLYNK_LINMOT_POWER_LOSS, (float)this->CO_powerLossWord);
  */

  wlog.log(DataParameter::ACTUAL_POSITON, (float)this->CO_actualPosition / pow(10, 4));
  wlog.log(DataParameter::ACTUAL_VELOCITY, (float)this->CO_actualVelocity / pow(10, 6));
}

// Task Wrappers
void linmot_heartbeat_task(void * pvParameter) {
  LinmotMotor* motor = (LinmotMotor*) pvParameter;

  while (true) {
    if (comm_canopen_is_ready()) {
      motor->task_heartbeat();
    }

    vTaskDelay(250 / portTICK_PERIOD_MS);
  }
}

static void linmot_run_task(void *pvParameter) {
  LinmotMotor* motor = (LinmotMotor*) pvParameter;

  // Wait for motor enable
  bool motorDisabled = true;
  while (motorDisabled) {
    motorDisabled = motor->hasStatusFlag(MOTOR_FLAG_ENABLED);
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
  
  // CO Startup must occur after motor CO configuration due to OD extension initialization
  CO_register_tasks();
  xTaskCreate(&linmot_heartbeat_task, "linmot_heartbeat_task", 4096, motor, 5, NULL);

  while (true) {
    if (!comm_canopen_is_ready()) {
      vTaskDelay(250 / portTICK_PERIOD_MS);
      continue;
    }

    motor->task_motion();
    vTaskDelay(100 / portTICK_PERIOD_MS);
  }
}

// TODO - move constructor to linmot.cpp?
LinmotMotor::LinmotMotor() {
  xTaskCreate(&linmot_run_task, "linmot_run_task", 4096, this, 5, NULL);
}