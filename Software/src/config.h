#ifndef CONFIG_H
#define CONFIG_H

#include "config_generic.h"
#include "config_user.h"

#if BOARD == BOARD_CANOPEN
  #include "config_canopen.h"
#elif BOARD == BOARD_OSSM_V2
  #include "config_ossmv2.h"
#endif

#endif