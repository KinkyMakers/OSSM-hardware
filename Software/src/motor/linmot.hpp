#ifndef MOTOR_LINMOT_H
#define MOTOR_LINMOT_H

#include <time.h>

#include "CANopen.h"
#include "motor/motor.h"

#define LINMOT_CONTROL_SWITCH_ON ((uint16_t)(1 << 0))
#define LINMOT_CONTROL_ERROR_ACKNOWLEDGE ((uint16_t)(1 << 7))
#define LINMOT_CONTROL_HOME ((uint16_t)(1 << 11))

#define LINMOT_STATUS_OPERATION_ENABLED (1 << 0)
#define LINMOT_STATUS_SWITCH_ON (1 << 1)
#define LINMOT_STATUS_ERROR (1 << 3)
#define LINMOT_STATUS_VOLTAGE_ENABLE (1 << 4)
#define LINMOT_STATUS_WARNING (1 << 7)
#define LINMOT_STATUS_AT_TARGET (1 << 10)
#define LINMOT_STATUS_HOMED (1 << 11)
#define LINMOT_STATUS_FATAL_ERROR (1 << 12)
#define LINMOT_STATUS_MOTION_ACTIVE (1 << 13)

#define LINMOT_WARN_MOTOR_HOT_SENSOR (1 << 0)
#define LINMOT_WARN_MOTOR_SHORT_TIME_OVERLOAD (1 << 1)
#define LINMOT_WARN_MOTOR_SUPPLY_VOLTAGE_LOW (1 << 2)
#define LINMOT_WARN_MOTOR_SUPPLY_VOLTAGE_HIGH (1 << 3)
#define LINMOT_WARN_MOTOR_DRIVE_HOT (1 << 6)
#define LINMOT_WARN_MOTOR_NOT_HOMED (1 << 7)

#define LINMOT_STATE_ERROR (0x04)
#define LINMOT_STATE_OPERATIONAL (0x08)
#define LINMOT_STATE_HOMING (0x09)

#define LINMOT_ERROR_MOTION_CMD_WRONG_STATE (0x86)
#define LINMOT_ERROR_CANBUS_GUARD_TIMEOUT (0xCD)

// Accepts four 16bit parameters 
// TODO - Define parameters better
#define LINMOT_CMD_VAI_16BIT_GO_TO_POS (0x0900)


#define MOTOR_FLAG_ENABLED (1 << 0)

#define MOTOR_FLAG_WARNING (1 << 1)
#define MOTOR_FLAG_ERROR (1 << 2)
#define MOTOR_FLAG_FATAL (1 << 3)

#define MOTOR_FLAG_AT_TARGET (1 << 4)
#define MOTOR_FLAG_MOTION_ACTIVE (1 << 5)
#define MOTOR_FLAG_HOMED (1 << 6)

typedef enum {
  INACTIVE = 0,
  ACTIVE = 1,
  HOMING = 2, 
  ERROR = 3
} MotorState;

class LinmotMotor: public MotorInterface {
  public:
    LinmotMotor();
    // Motion
    void goToHome() { this->state = MotorState::HOMING; this->removeStatusFlag(MOTOR_FLAG_HOMED); }
    void stopMotion();

    // General
    void enable() { this->addStatusFlag(MOTOR_FLAG_ENABLED); }
    void registerTasks();

    // CANOpen
    void CO_setNodeId(uint8_t nodeId);
    void CO_setControl(OD_entry_t *entry);
    void CO_setCmdHeader(OD_entry_t *entry);
    void CO_setCmdParameters(OD_entry_t *entry);
    void CO_setStatusUInt16(OD_entry_t *entry);
    void CO_setMonitorUInt16(OD_entry_t *entry); // Not used yet, merged with Status UInt16 at the moment
    void CO_setMonitorSInt16(OD_entry_t *entry);
    void CO_setMonitorSInt32(OD_entry_t *entry);

    void CO_control_addFlag(uint16_t flag);
    void CO_control_removeFlag(uint16_t flag);

    void CO_sendCmd(uint16_t cmd, uint16_t parameter_a, uint16_t parameter_b, uint16_t parameter_c, uint16_t parameter_d);

    void CO_run_rpdo_received();
    void CO_motion_rpdo_received();
    void CO_monitor_rpdo_received();

    // Tasks
    void task_motion();
    void task_heartbeat();

    // Status / State flags
    MotorState getState() { return this->state; }
    const char* getStateString() {
      if (this->state == MotorState::INACTIVE) { return "INACTIVE"; }
      if (this->state == MotorState::ACTIVE) { return "ACTIVE"; }
      if (this->state == MotorState::HOMING) { return "HOMING"; }
      if (this->state == MotorState::ERROR) { return "ERROR"; }

      return "UNKNOWN";
    }
    uint32_t getStatus() { return this->status; }
    bool isInState(MotorState state) { return this->state == state; }
    bool hasStatusFlag(uint32_t flag) { return (this->status & flag) > 0; }

  protected:
    void unsafeGoToPos(float position, float speed, float acceleration);
    
    MotorState state = MotorState::INACTIVE;
    uint32_t status = 0;

    void addStatusFlag(uint32_t flag) { this->status |= flag; }
    void removeStatusFlag(uint32_t flag) { this->status &= ~flag; }

  private:
    uint8_t CO_nodeId;
    
    OD_entry_t *CO_statusWord_entry;
    OD_extension_t CO_statusWord_extension;
    
    OD_entry_t *CO_monitorUInt16_entry;
    OD_extension_t CO_monitorUInt16_extension;
    
    OD_entry_t *CO_monitorSInt16_entry;
    OD_extension_t CO_monitorSInt16_extension;
    
    OD_entry_t *CO_monitorSInt32_entry;
    OD_extension_t CO_monitorSInt32_extension;

    OD_entry_t *CO_controlWord_entry;
    OD_extension_t CO_controlWord_extension;
    uint8_t *CO_controlWord_flags;

    OD_entry_t *CO_cmdHeader_entry;

    OD_entry_t *CO_cmdParameters_entry;
    OD_extension_t CO_cmdParameters_extension;
    uint8_t *CO_cmdParameters_flags;

    uint16_t CO_controlWord = 0x003E;

    // TPDO 1
    uint16_t CO_statusWord;
    uint16_t CO_runWord;
    uint16_t CO_errorWord;
    uint16_t CO_warnWord;

    // TPDO 2
    int32_t CO_actualPosition;  // 0.0001mm Scale
    int32_t CO_actualVelocity;  // 1E-6 m/s Scale

    // TPDO 3
    uint16_t CO_motorVoltage;   // 0.01 V Scale
    int16_t CO_modelTemp;       // 0.1 C Scale
    uint16_t CO_realTemp;       // 0.980392156862745 C Scale, -50 C Offset
    uint16_t CO_powerLoss;      // 0.1 W Scale

    // TPDO 4
    int32_t CO_actualForce;     // 0.1 N Scale

    uint8_t CO_cmdCount = 0;
    uint16_t position = 0;
    bool hasInitialized = false;
    time_t lastRPDOUpdate;
};

#endif