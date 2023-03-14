#ifndef CO_MAIN_H
#define CO_MAIN_H

#include "CANopen.h"
#include "301/CO_config.h"
#include "OD.h"

#define CANOPEN_TASK_INTERVAL_MS 100
#define CANOPEN_PROCESS_INTERVAL_US 1000

extern CO_t *CO;

bool comm_canopen_is_ready();
void CO_register_tasks();

#endif