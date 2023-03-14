#include "motor/linmot.hpp"

void LinmotMotor::unsafeGoToPos(float position, float speed, float acceleration) {
  if (!this->isInState(MotorState::ACTIVE)) {
    ESP_LOGE("LinmotMotor", "Attempted to issue Motion CMD while in incorrect state '%s'!", this->getStateString());
    return;
  }

  this->CO_sendCmd(
    0x0900, 
    static_cast<uint16_t>(this->_mapSafePosition(position) * 10), 
    static_cast<uint16_t>(speed), 
    static_cast<uint16_t>(acceleration), 
    static_cast<uint16_t>(acceleration)
  );
}

void LinmotMotor::stopMotion() {
  
}