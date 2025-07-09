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
static TaskHandle_t drawPatternControlsTaskH = nullptr;
static TaskHandle_t wmTaskH = nullptr;
static TaskHandle_t drawPreflightTaskH = nullptr;

static TaskHandle_t runHomingTaskH = nullptr;
static TaskHandle_t runSimplePenetrationTaskH = nullptr;
static TaskHandle_t runStrokeEngineTaskH = nullptr;
static TaskHandle_t currentMonitoringTaskH = nullptr;

static const int stepperCore = 1;
static const int operationTaskCore = 0;

#endif  // OSSM_SOFTWARE_TASKS_H
