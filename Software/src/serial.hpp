#pragma once

#include "config.h"
#if VCP_AVAILABLE == 1
void serial_setup();
void serial_task(void* pvParameter);
#endif