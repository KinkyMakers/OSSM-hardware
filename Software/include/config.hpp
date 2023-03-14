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
/*************** START USER CONFIGURATION ************/
/*****************************************************/
#define BOARD BOARD_CANOPEN
#define MOTOR_TYPE MOTOR_TYPE_LINMOT

// Addon: Eject Cum Pump - https://github.com/ortlof/EJECT-cum-tube-project
#define ENABLE_ADDON_EJECT FUNCTIONALITY_DISABLED
#define ADDON_EJECT_MOTOR_STEP_PIN 4
#define ADDON_EJECT_MOTOR_DIRECTION_PIN 5
#define ADDON_EJECT_MOTOR_ENA_PIN 6

#define ADDON_EJECT_CUMSPEED  20
#define ADDON_EJECT_CUMTIME   21
#define ADDON_EJECT_CUMSIZE   22
#define ADDON_EJECT_CUMACCEL  23

/*****************************************************/
/**************** END USER CONFIGURATION *************/
/*****************************************************/

/*****************************************************/
/*************** INTERNAL CONFIGURATION **************/
/*****************************************************/
/*!!!!!!!!!! DO NOT TOUCH UNLESS INSTRUCTED !!!!!!!!!*/

#define UART_SPEED 115200
#define M5_REMOTE_ADDRESS {0x08, 0x3A, 0xF2, 0x68, 0x1E, 0x74}

#if BOARD == BOARD_CANOPEN
  #include "config_canopen.hpp"
#elif BOARD == BOARD_OSSM_V2
  #include "config_ossmv2"
#endif