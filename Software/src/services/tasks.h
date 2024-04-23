#ifndef OSSM_SOFTWARE_TASKS_H
#define OSSM_SOFTWARE_TASKS_H

#include <freertos/task.h>

/**
 * ///////////////////////////////////////////
 * ////
 * ////  Private Objects and Services
 * ////
 * ///////////////////////////////////////////
 */

static TaskHandle_t displayTask = nullptr;
static TaskHandle_t operationTask = nullptr;

#endif  // OSSM_SOFTWARE_TASKS_H
