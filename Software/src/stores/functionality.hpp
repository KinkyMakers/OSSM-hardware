#pragma once
#include "StatefulService.hpp"
#include "config.h"

class FunctionalityState {
  public:
    // xAvailable const members are not written to/read from flash as they describe board-level functionality
    const bool modbusAvailable = MODBUS_AVAILABLE; 
    bool modbusEnabled = MODBUS_DEFAULT;
    uint8_t modbusRxPin = MODBUS_RX_PIN;
    uint8_t modbusTxPin = MODBUS_TX_PIN;
    uint32_t modbusBaudRate = MODBUS_BAUD_RATE;

    const bool canbusAvailable = CANOPEN_AVAILABLE;
    bool canbusEnabled = CANOPEN_DEFAULT;
    uint8_t canbusRxPin = CANOPEN_RX_PIN;
    uint8_t canbusTxPin = CANOPEN_TX_PIN;

    bool vcpEnabled = VCP_DEFAULT;

    bool lvglEnabled = LVGL_DEFAULT;

  bool on = false;
  uint8_t brightness = 255;
};

class FunctionalityStateService : public StatefulService<FunctionalityState> {

};

namespace Config {
  extern FunctionalityStateService functionality;
};