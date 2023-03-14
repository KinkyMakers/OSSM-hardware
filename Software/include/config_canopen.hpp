#pragma once

#define ENABLE_CANOPEN FUNCTIONALITY_ENABLED
#define ENABLE_LVGL FUNCTIONALITY_ENABLED

#define ENABLE_VCP 1
#define VCP_BUFFER_SIZE 1024
#define VCP_BAUD_RATE 38400
#define VCP_RX_PIN 44
#define VCP_TX_PIN 43

#define ENABLE_LVGL FUNCTIONALITY_ENABLED

#if MOTOR_TYPE == MOTOR_TYPE_LINMOT
  #if ENABLE_CANOPEN != 1 
  #error "CANopen must be enabled when using a LinMot motor!");
  #endif
#endif
