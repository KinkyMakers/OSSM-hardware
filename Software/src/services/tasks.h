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

static const int displayTaskCore = 0;
static const int operationTaskCore = 1;

#endif  // OSSM_SOFTWARE_TASKS_H
