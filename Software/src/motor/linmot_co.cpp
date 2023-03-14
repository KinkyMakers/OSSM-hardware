#include "motor/linmot.hpp"
#include "esp_log.h"

ODR_t track_status_update(
  OD_stream_t *stream, const void *buf,
  OD_size_t count, OD_size_t *countWritten
) {
  ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);

  // TODO - Determine which RPDO was updated
  ((LinmotMotor*)(stream->object))->CO_run_rpdo_received();

  return ret;
};

// TODO - Not really a clean status / monitor split. Real Temp is in Status Update
ODR_t track_monitor_update(
  OD_stream_t *stream, const void *buf,
  OD_size_t count, OD_size_t *countWritten
) {
  ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);

  // TODO - Determine which RPDO was updated
  ((LinmotMotor*)(stream->object))->CO_motion_rpdo_received();
  ((LinmotMotor*)(stream->object))->CO_run_rpdo_received();

  return ret;
};

// TODO - Can we move some of this logic out to somewhere better?
// NOTE - This is a time sensitive method. If it doesn't return in enough time, CANOpen Timeout errors will start occuring with the LinMot
void LinmotMotor::CO_run_rpdo_received () {
  OD_get_u16(this->CO_statusWord_entry, 0x01, &this->CO_statusWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x02, &this->CO_runWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x03, &this->CO_errorWord, false);
  OD_get_u16(this->CO_statusWord_entry, 0x04, &this->CO_warnWord, false);

  uint16_t statusWord = this->CO_statusWord;
  // uint16_t errorWord = this->CO_errorWord;

  if ((statusWord & LINMOT_STATUS_HOMED) > 0) { this->addStatusFlag(MOTOR_FLAG_HOMED); } else { this->removeStatusFlag(MOTOR_FLAG_HOMED); }
  if ((statusWord & LINMOT_STATUS_MOTION_ACTIVE) > 0) { this->addStatusFlag(MOTOR_FLAG_MOTION_ACTIVE); } else { this->removeStatusFlag(MOTOR_FLAG_MOTION_ACTIVE); }
  if ((statusWord & LINMOT_STATUS_AT_TARGET) > 0) { this->addStatusFlag(MOTOR_FLAG_AT_TARGET); } else { this->removeStatusFlag(MOTOR_FLAG_AT_TARGET); }

  if ((statusWord & LINMOT_STATUS_WARNING) > 0) { this->addStatusFlag(MOTOR_FLAG_WARNING); } else { this->removeStatusFlag(MOTOR_FLAG_WARNING); }
  if ((statusWord & LINMOT_STATUS_ERROR) > 0) { this->addStatusFlag(MOTOR_FLAG_ERROR); } else { this->removeStatusFlag(MOTOR_FLAG_ERROR); }
  if ((statusWord & LINMOT_STATUS_FATAL_ERROR) > 0) { this->addStatusFlag(MOTOR_FLAG_FATAL); } else { this->removeStatusFlag(MOTOR_FLAG_FATAL); }

  uint8_t runState = (this->CO_runWord & 0xff00) >> 8;
  if (runState == LINMOT_STATE_ERROR) {
    this->state = MotorState::ERROR;
  } else if (runState == LINMOT_STATE_HOMING) {
    this->state = MotorState::HOMING;
  } else if (runState == LINMOT_STATE_OPERATIONAL) {
    this->state = MotorState::ACTIVE;
  }

  // ESP_LOGI("task.main", "RPDO CSword %d, CRunword %d, RunState %d, CErrorword %d,  CWarnword %d, Status %d, State %d!", this->CO_statusWord, this->CO_runWord, runState, this->CO_errorWord, this->CO_warnWord, this->status, (int)this->getState());

  time(&this->lastRPDOUpdate);
}

void LinmotMotor::CO_motion_rpdo_received () {
  OD_get_i32(this->CO_monitorSInt32_entry, 0x01, &this->CO_actualPosition, false);
  OD_get_i32(this->CO_monitorSInt32_entry, 0x02, &this->CO_actualVelocity, false);
  OD_get_i32(this->CO_monitorSInt32_entry, 0x03, &this->CO_actualForce, false);
}

void LinmotMotor::CO_monitor_rpdo_received () {
  OD_get_i16(this->CO_monitorSInt16_entry, 0x01, &this->CO_modelTemp, false);
  OD_get_u16(this->CO_statusWord_entry, 0x07, &this->CO_realTemp, false);
  OD_get_u16(this->CO_statusWord_entry, 0x05, &this->CO_motorVoltage, false);
  OD_get_u16(this->CO_statusWord_entry, 0x06, &this->CO_powerLoss, false);
}

void LinmotMotor::CO_setNodeId(uint8_t nodeId) {
  this->CO_nodeId = nodeId;
};

void LinmotMotor::CO_setControl(OD_entry_t *entry) {
  this->CO_controlWord_entry = entry;
  this->CO_controlWord_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = OD_writeOriginal
  };

  OD_extension_init(entry, &this->CO_controlWord_extension);
  this->CO_controlWord_flags = OD_getFlagsPDO(entry);
};

void LinmotMotor::CO_setCmdHeader(OD_entry_t *entry) {
  this->CO_cmdHeader_entry = entry;
};

void LinmotMotor::CO_setCmdParameters(OD_entry_t *entry) {
  this->CO_cmdParameters_entry = entry;
  this->CO_cmdParameters_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = OD_writeOriginal
  };

  OD_extension_init(entry, &this->CO_cmdParameters_extension);
  this->CO_cmdParameters_flags = OD_getFlagsPDO(entry);
};

// TODO - Should we separate these into proper categories. Status, MonitorUInt16, MonitorSInt16, etc?
void LinmotMotor::CO_setStatusUInt16(OD_entry_t *entry) {
  this->CO_statusWord_entry = entry;
  this->CO_statusWord_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = track_status_update
  };

  OD_extension_init(entry, &this->CO_statusWord_extension);
};

void LinmotMotor::CO_setMonitorUInt16(OD_entry_t *entry) {
  this->CO_monitorUInt16_entry = entry;
  this->CO_monitorUInt16_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = track_monitor_update
  };

  OD_extension_init(entry, &this->CO_monitorUInt16_extension);
};

void LinmotMotor::CO_setMonitorSInt16(OD_entry_t *entry) {
  this->CO_monitorSInt16_entry = entry;
  this->CO_monitorSInt16_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = track_monitor_update
  };

  OD_extension_init(entry, &this->CO_monitorSInt16_extension);
};

void LinmotMotor::CO_setMonitorSInt32(OD_entry_t *entry) {
  this->CO_monitorSInt32_entry = entry;
  this->CO_monitorSInt32_extension = {
    .object = this,
    .read = OD_readOriginal,
    .write = track_monitor_update
  };

  OD_extension_init(entry, &this->CO_monitorSInt32_extension);
};

void LinmotMotor::CO_control_addFlag(uint16_t flag) {
    this->CO_controlWord |= flag;
    OD_set_u16(this->CO_controlWord_entry, 0x00, this->CO_controlWord, false);
    OD_requestTPDO(this->CO_controlWord_flags, 0);
}

void LinmotMotor::CO_control_removeFlag(uint16_t flag) {
    this->CO_controlWord &= ~flag;
    OD_set_u16(this->CO_controlWord_entry, 0x00, this->CO_controlWord, false);
    OD_requestTPDO(this->CO_controlWord_flags, 0);
}

void LinmotMotor::CO_sendCmd(uint16_t cmd, uint16_t parameter_a, uint16_t parameter_b, uint16_t parameter_c, uint16_t parameter_d) {
  this->CO_controlWord = (this->CO_controlWord + 1) % 16;
  uint16_t cmdWord = (cmd & 0xFFF0) | (this->CO_controlWord & 0x0F);

  //ESP_LOGI("task.main", "Sending LinMot Motion CMD %d, %d!\n", cmdWord, parameter_a);
  OD_set_u16(this->CO_cmdHeader_entry, 0x00, cmdWord, false);

  OD_set_u8(this->CO_cmdParameters_entry, 0x01, (uint8_t)((parameter_a >> 0) & 0xFF), false);
  OD_set_u8(this->CO_cmdParameters_entry, 0x02, (uint8_t)((parameter_a >> 8) & 0xFF), false);

  OD_set_u8(this->CO_cmdParameters_entry, 0x03, (uint8_t)((parameter_b >> 0) & 0xFF), false);
  OD_set_u8(this->CO_cmdParameters_entry, 0x04, (uint8_t)((parameter_b >> 8) & 0xFF), false);

  OD_set_u8(this->CO_cmdParameters_entry, 0x05, (uint8_t)((parameter_c >> 0) & 0xFF), false);
  OD_set_u8(this->CO_cmdParameters_entry, 0x06, (uint8_t)((parameter_c >> 8) & 0xFF), false);

  OD_set_u8(this->CO_cmdParameters_entry, 0x07, (uint8_t)((parameter_d >> 0) & 0xFF), false);
  OD_set_u8(this->CO_cmdParameters_entry, 0x08, (uint8_t)((parameter_d >> 8) & 0xFF), false);

  // Triggers both TDPO 1 & 2
  OD_requestTPDO(this->CO_cmdParameters_flags, 0x01);
  OD_requestTPDO(this->CO_cmdParameters_flags, 0x07);
}