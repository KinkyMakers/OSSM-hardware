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

    xTaskCreatePinnedToCore(
        currentMonitoringTask,
        "CurrentMonitor", 
        4096,
        ossm, 
        configMAX_PRIORITIES - 3,
        &currentMonitoringTaskH, 
        1
    );
    
    if (currentMonitoringTaskH == nullptr) {
        ESP_LOGE("UTILS", "Failed to create current monitoring task!");
    }

    while (isInCorrectState(ossm)) {
        if (isChangeSignificant(lastSetting.speed, ossm->setting.speed)) {
            if (ossm->setting.speed == 0) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(ossm->setting.speed * 3, true);
            lastSetting.speed = ossm->setting.speed;
        }
        
        // Check for force safety trigger
        static bool lastForceState = false;
        bool currentForceState = ossm->isForceSafetyTriggered();
        
        if (currentForceState && !lastForceState) {
            ESP_LOGD("UTILS", "Force safety event processed");
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

        vTaskDelay(10);
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
    const unsigned long MIN_TRIGGER_INTERVAL_MS = 50;

    auto isInCorrectState = [](OSSM *ossm) {
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s) ||
               ossm->sm->is("strokeEngine.pattern"_s);
    };

    ESP_LOGD("UTILS", "Current monitoring task started");

    FastAccelStepper* stepperPtr = ossm->stepper;
    
    static float lastThresholdSetting = -1.0f;
    static float cachedThresholdFactor = 0.0f;
    static bool cachedThresholdEnabled = false;
    
    while (isInCorrectState(ossm)) {
        try {
            if (ossm->setting.currentThreshold != lastThresholdSetting) {
                lastThresholdSetting = ossm->setting.currentThreshold;
                cachedThresholdEnabled = (ossm->setting.currentThreshold < 100);
                cachedThresholdFactor = ossm->setting.currentThreshold / 100.0f * 50.0f;
            }
            
            float currentReading = getAnalogAveragePercent(
                        SampleOnPin{Pins::Driver::currentSensorPin, 3}) -
                    ossm->currentSensorOffset;

            ossm->lastCurrentReading = currentReading;

            unsigned long currentTime = millis();
            
            if (cachedThresholdEnabled) {
                bool thresholdExceeded = (currentReading > cachedThresholdFactor);
                
                if (thresholdExceeded) {
                    bool canTrigger = (currentTime - lastTriggerTime) > MIN_TRIGGER_INTERVAL_MS;
                    
                    if (canTrigger) {
                        int32_t currentPosition = stepperPtr->getCurrentPosition();
                        int32_t currentTarget = stepperPtr->targetPos();
                        
                        bool isForwardMove = (currentTarget > currentPosition);
                        
                        if (isForwardMove) {
                            ESP_LOGW("UTILS", "Force threshold exceeded - stopping forward move at pos %d", currentPosition);
                            
                            bool stopAndAdvanceResult = Stroker.forceStopAndAdvance();
                            if (!stopAndAdvanceResult) {
                                ESP_LOGE("UTILS", "Failed to stop and advance pattern");
                            }
                            
                            ossm->setForceSafetyTriggered(true);
                            lastTriggerTime = currentTime;
                        }
                    }
                }
            }
        } catch (...) {
            ESP_LOGE("UTILS", "Exception in current monitoring task, continuing...");
        }

        // Monitoring frequency of 1ms for responsiveness
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