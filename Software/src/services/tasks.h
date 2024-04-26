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

static TaskHandle_t drawHelloTaskH = nullptr;
static TaskHandle_t drawMenuTaskH = nullptr;
static TaskHandle_t drawPlayControlsTaskH = nullptr;

static TaskHandle_t runHomingTaskH = nullptr;
static TaskHandle_t runSimplePenetrationTaskH = nullptr;

static const int displayTaskCore = 1;
static const int operationTaskCore = 0;

#endif  // OSSM_SOFTWARE_TASKS_H
