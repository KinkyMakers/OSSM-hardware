#include "OSSM.h"

#include "services/stepper.h"

void OSSM::startStrokeEngineTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    float measuredStrokeMm = ossm->measuredStrokeSteps / (1_mm);

    machineGeometry strokingMachine = {
        .physicalTravel = abs(ossm->measuredStrokeSteps / (1_mm)),
        .keepoutBoundary = 6.0};
    SettingPercents lastSetting = ossm->setting;

    Stroker.begin(&strokingMachine, &servoMotor, ossm->stepper);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(ossm->setting.sensation), true);

    Stroker.setDepth(0.01f * ossm->setting.depth * abs(measuredStrokeMm), true);
    Stroker.setStroke(0.01f * ossm->setting.stroke * abs(measuredStrokeMm),
                      true);
    Stroker.moveToMax(10 * 3);

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s) ||
               ossm->sm->is("strokeEngine.pattern"_s);
    };

    // Start the high-frequency current monitoring task
    ESP_LOGD("UTILS", "Starting current monitoring task...");
    xTaskCreatePinnedToCore(
        currentMonitoringTask,
        "CurrentMonitor", 
        4096, // Adequate stack size for stability
        ossm, 
        configMAX_PRIORITIES - 3, // Lower priority to avoid starving system tasks
        &currentMonitoringTaskH, 
        1 // Pin to same core as stroke engine
    );
    
    if (currentMonitoringTaskH == nullptr) {
        ESP_LOGE("UTILS", "Failed to create current monitoring task!");
    } else {
        ESP_LOGD("UTILS", "Current monitoring task created successfully");
    }

    while (isInCorrectState(ossm)) {
        // Check for force safety trigger first
        if (ossm->isForceSafetyTriggered()) {
            ESP_LOGD("UTILS", "Force safety active - pattern will skip forward moves until flag is cleared");
            
            // Clear the force flag after a short timeout to allow pattern to resume
            static unsigned long forceStartTime = 0;
            static bool forceTimeSet = false;
            
            if (!forceTimeSet) {
                forceStartTime = millis();
                forceTimeSet = true;
            }
            
            // Clear flag after 1 second to allow pattern to resume faster
            if ((millis() - forceStartTime) > 1000) {
                ESP_LOGD("UTILS", "Force timeout reached, clearing force flag");
                ossm->setForceSafetyTriggered(false);
                forceTimeSet = false;
            }
        } else {
            // Reset force time tracking when not in force mode
            static bool forceTimeSet = false;
            forceTimeSet = false;
        }
        
        if (isChangeSignificant(lastSetting.speed, ossm->setting.speed)) {
            if (ossm->setting.speed == 0) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(ossm->setting.speed * 3, true);
            lastSetting.speed = ossm->setting.speed;
        }
        
        // Check for force safety trigger and handle forward move cancellation
        static bool lastForceState = false;
        bool currentForceState = ossm->isForceSafetyTriggered();
        
        if (currentForceState && !lastForceState) {
            // Force safety just triggered - the current monitoring task has already stopped the stepper
            ESP_LOGD("UTILS", "Force safety triggered - forward move has been stopped, pattern will advance naturally");
            
            // Clear the force flag since the move has been stopped
            ossm->setForceSafetyTriggered(false);
        }
        
        lastForceState = currentForceState;

        if (lastSetting.stroke != ossm->setting.stroke) {
            float newStroke =
                0.01f * ossm->setting.stroke * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", ossm->setting.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = ossm->setting.stroke;
        }

        if (lastSetting.depth != ossm->setting.depth) {
            float newDepth =
                0.01f * ossm->setting.depth * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change depth: %f %f", ossm->setting.depth,
                     newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = ossm->setting.depth;
        }

        if (lastSetting.sensation != ossm->setting.sensation) {
            float newSensation = calculateSensation(ossm->setting.sensation);
            ESP_LOGD("UTILS", "change sensation: %f %f",
                     ossm->setting.sensation, newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = ossm->setting.sensation;
        }

        if (lastSetting.currentThreshold != ossm->setting.currentThreshold) {
            ESP_LOGD("UTILS", "change current threshold: %f", 
                     ossm->setting.currentThreshold);
            lastSetting.currentThreshold = ossm->setting.currentThreshold;
        }

        if (lastSetting.pattern != ossm->setting.pattern) {
            ESP_LOGD("UTILS", "change pattern: %d", ossm->setting.pattern);

            switch (ossm->setting.pattern) {
                case StrokePatterns::SimpleStroke:
                    Stroker.setPattern(new SimpleStroke("Simple Stroke"),
                                       false);
                    break;
                case StrokePatterns::TeasingPounding:
                    Stroker.setPattern(new TeasingPounding("Teasing Pounding"),
                                       false);
                    break;
                case StrokePatterns::RoboStroke:
                    Stroker.setPattern(new RoboStroke("Robo Stroke"), false);
                    break;
                case StrokePatterns::HalfnHalf:
                    Stroker.setPattern(new HalfnHalf("Half'n'Half"), false);
                    break;
                case StrokePatterns::Deeper:
                    Stroker.setPattern(new Deeper("Deeper"), false);
                    break;
                case StrokePatterns::StopNGo:
                    Stroker.setPattern(new StopNGo("Stop'n'Go"), false);
                    break;
                case StrokePatterns::Insist:
                    Stroker.setPattern(new Insist("Insist"), false);
                    break;
                default:
                    break;
            }

            lastSetting.pattern = ossm->setting.pattern;
        }

        vTaskDelay(10); // Balanced 10ms for responsive safety checks without starving UI
    }

    // Clean up current monitoring task
    if (currentMonitoringTaskH != NULL) {
        vTaskDelete(currentMonitoringTaskH);
        currentMonitoringTaskH = NULL;
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void OSSM::currentMonitoringTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    float measuredStrokeMm = ossm->measuredStrokeSteps / (1_mm);
    unsigned long lastTriggerTime = 0;
    const unsigned long MIN_TRIGGER_INTERVAL_MS = 0; // No minimum interval for maximum responsiveness

    auto isInCorrectState = [](OSSM *ossm) {
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s) ||
               ossm->sm->is("strokeEngine.pattern"_s);
    };

    ESP_LOGD("UTILS", "Current monitoring task started with threshold: %f", ossm->setting.currentThreshold);
    ESP_LOGW("UTILS", "Current sensor offset: %f", ossm->currentSensorOffset);

    // Cache stepper pointers for faster access
    FastAccelStepper* stepperPtr = ossm->stepper;
    
    // Dynamic threshold caching - recalculate only when setting changes
    static float lastThresholdSetting = -1.0f;
    static float cachedThresholdFactor = 0.0f;
    static bool cachedThresholdEnabled = false;
    
    while (isInCorrectState(ossm)) {
        try {
            // Update threshold cache if setting changed (check every iteration)
            if (ossm->setting.currentThreshold != lastThresholdSetting) {
                lastThresholdSetting = ossm->setting.currentThreshold;
                cachedThresholdEnabled = (ossm->setting.currentThreshold < 100);
                cachedThresholdFactor = ossm->setting.currentThreshold / 100.0f * 50.0f; //Current sensor max ~50
                ESP_LOGW("UTILS", "Threshold cache updated: %f%% -> factor=%f, enabled=%s", 
                        lastThresholdSetting, cachedThresholdFactor, cachedThresholdEnabled ? "true" : "false");
            }
            
            // Fast current reading with minimal averaging for noise reduction
            // 3 samples provides good balance between speed and noise immunity
            float currentReading = getAnalogAveragePercent(
                        SampleOnPin{Pins::Driver::currentSensorPin, 3}) -
                    ossm->currentSensorOffset;

            // Store the current reading for display purposes
            ossm->lastCurrentReading = currentReading;

            // Debug: Always show current vs threshold for troubleshooting
            static unsigned long lastDebugTime = 0;
            unsigned long currentTime = millis();
            if ((currentTime - lastDebugTime) > 1000) { // Reduced frequency to save CPU for monitoring
                int32_t currentPosition = stepperPtr->getCurrentPosition();
                int32_t currentTarget = stepperPtr->targetPos();
                bool isMoving = stepperPtr->isRunning();
                bool thresholdExceeded = cachedThresholdEnabled && (currentReading > cachedThresholdFactor);
                ESP_LOGW("UTILS", "Current monitoring DEBUG: threshold=%f%% (factor=%f), current=%f, exceeded=%s, enabled=%s, forceFlag=%s, pos=%d, target=%d, moving=%s", 
                         ossm->setting.currentThreshold, cachedThresholdFactor, currentReading, 
                         thresholdExceeded ? "true" : "false",
                         cachedThresholdEnabled ? "true" : "false",
                         ossm->isForceSafetyTriggered() ? "true" : "false",
                         currentPosition, currentTarget, isMoving ? "true" : "false");
                lastDebugTime = currentTime;
            }

            // Fast path: Only check threshold if enabled and not already triggered
            if (cachedThresholdEnabled && !ossm->isForceSafetyTriggered()) {
                bool thresholdExceeded = (currentReading > cachedThresholdFactor);
                
                if (thresholdExceeded) {
                    bool canTrigger = (currentTime - lastTriggerTime) > MIN_TRIGGER_INTERVAL_MS;
                    
                    if (canTrigger) {
                        // Get current position and target for direction analysis
                        // Use direct stepper calls for minimum latency
                        int32_t currentPosition = stepperPtr->getCurrentPosition();
                        int32_t currentTarget = stepperPtr->targetPos();
                        
                        // Forward move is any move where target > current position (away from home)
                        bool isForwardMove = (currentTarget > currentPosition);
                        
                        if (isForwardMove) {
                            ESP_LOGW("UTILS", "*** CURRENT THRESHOLD TRIGGERED ON FORWARD MOVE *** Current: %f, Threshold: %f%% (actual=%f), Pos: %d, Target: %d - IMMEDIATELY stopping forward move",
                                        currentReading, ossm->setting.currentThreshold, cachedThresholdFactor, currentPosition, currentTarget);
                            
                            // Additional logging for home position debugging
                            if (abs(currentPosition) < 100) { // Near home position
                                ESP_LOGW("UTILS", "*** FORWARD MOVE FROM NEAR HOME DETECTED AND STOPPED *** Pos: %d -> Target: %d", currentPosition, currentTarget);
                            }
                            
                            // IMMEDIATELY stop the forward move to prevent completion
                            stepperPtr->forceStop();
                            
                            // Set force safety flag to signal the main task
                            ossm->setForceSafetyTriggered(true);
                            lastTriggerTime = currentTime;
                        } else {
                            ESP_LOGD("UTILS", "Current threshold exceeded but move is toward home (safe) - Pos: %d, Target: %d - allowing move to continue",
                                    currentPosition, currentTarget);
                        }
                    }
                }
            }
        } catch (...) {
            ESP_LOGE("UTILS", "Exception in current monitoring task, continuing...");
        }

        // Balanced monitoring frequency of 1ms for reasonable responsiveness and system impact
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    ESP_LOGD("UTILS", "Current monitoring task exiting");
    vTaskDelete(NULL);
}

void OSSM::startStrokeEngine() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, this, configMAX_PRIORITIES - 1,
                            &runStrokeEngineTaskH, operationTaskCore);
}