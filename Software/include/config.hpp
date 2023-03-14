#pragma once

#define FUNCTIONALITY_ENABLED 1
#define FUNCTIONALITY_DISABLED 0

#define BOARD_CANOPEN "CANopen"
#define BOARD_OSSM_V2 "OSSMv2"

#define MOTOR_TYPE_STEPPER "Stepper"
#define MOTOR_TYPE_IHSV_5 "iHSV5"
#define MOTOR_TYPE_IHSV_6 "iHSV6"
#define MOTOR_TYPE_LINMOT "LinMot"

/*****************************************************/
/*******************USER CONFIGURATION****************/
/*****************************************************/
#define BOARD BOARD_CANOPEN
#define MOTOR_TYPE MOTOR_TYPE_LINMOT
#define UART_SPEED 
/*****************************************************/
/*****************************************************/

#if BOARD == BOARD_CANOPEN
  #include "config_canopen.hpp"
#elif BOARD == BOARD_OSSM_V2
  #include "config_ossmv2"
#endif