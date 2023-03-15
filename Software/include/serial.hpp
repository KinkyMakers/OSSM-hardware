#pragma once

#include "config.hpp"
#if VCP_AVAILABLE == 1
void serial_setup();
void serial_task(void* pvParameter);
#endif