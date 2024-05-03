#include "OSSM.h"

// #include "StrokeEngine.h"
// #include "utils/StrokeEngineHelper.h"

void OSSM::startStrokeEngineTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;
    ossm->stepper.stopService();

    int fullStrokeCount = 0;

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("strokeEngine"_s) ||
               ossm->sm->is("strokeEngine.idle"_s);
    };

    while (isInCorrectState(ossm)) {
    }

    vTaskDelete(nullptr);
}

void OSSM::startStrokeEngine() {
    int stackSize = 15 * configMINIMAL_STACK_SIZE;

//    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
//                            stackSize, this, configMAX_PRIORITIES - 1,
//                            &runStrokeEngineTaskH, operationTaskCore);

    //    String strokerPatternName = "";
    //    static int strokePattern = 0;
    //    static int strokePatternCount = 0;
    //    static float depthPercentage = 50;
    //
    //    static float sensationPercentage = 50;
    //    static int encoderButtonPresses = 0;
    //    static long lastEncoderButtonPressMillis = 0;
    //    static bool modeChanged = false;
    //
    //    float lastSpeedPercentage = speedPercentage;
    //    long lastStrokePercentage = strokePercentage;
    //    float lastDepthPercentage = depthPercentage;
    //    float lastSensationPercentage = sensationPercentage;
    //    int lastEncoderButtonPresses = encoderButtonPresses;
    //
    //    machineGeometry strokingMachine = {.physicalTravel =
    //    abs(measuredStrokeMm),
    //                                       .keepoutBoundary = 6.0};
    //    class StrokeEngine Stroker;
    //
    //    Stroker.begin(&strokingMachine, &servoMotor);
    //    Stroker.thisIsHome();
    //
    //    strokePatternCount = Stroker.getNumberOfPattern();
    //
    //    Stroker.setSensation(calculateSensation(sensationPercentage), true);
    //
    //    Stroker.setPattern(int(strokePattern), true);
    //    Stroker.setDepth(0.01f * depthPercentage * abs(measuredStrokeMm),
    //    true); Stroker.setStroke(0.01f * strokePercentage *
    //    abs(measuredStrokeMm), true); Stroker.moveToMax(10 * 3);
    //
    //    ESP_LOGD("UTILS", "Stroker State: %d", Stroker.getState());
    //
    //    strokerPatternName = Stroker.getPatternName(
    //        strokePattern);  // Set the initial stroke engine pattern name
    //
    //    for (;;) {
    //        ESP_LOGV("UTILS", "Looping");
    //        if (isChangeSignificant(lastSpeedPercentage, speedPercentage)) {
    //            ESP_LOGD("UTILS", "changing speed: %f", speedPercentage * 3);
    //            if (speedPercentage == 0) {
    //                Stroker.stopMotion();
    //            } else if (Stroker.getState() == READY) {
    //                Stroker.startPattern();
    //            }
    //
    //            Stroker.setSpeed(speedPercentage * 3,
    //                             true);  // multiply by 3 to get to sane
    //                             thrusts
    //                                     // per minute speed
    //            lastSpeedPercentage = speedPercentage;
    //        }
    //
    //        int buttonPressCount = encoderButtonPresses -
    //        lastEncoderButtonPresses;
    //        //        if (!modeChanged && buttonPressCount > 0 &&
    //        //            (millis() - lastEncoderButtonPressMillis) > 200) {
    //        //            ESP_LOGD("UTILS", "switching mode pre: %i %i",
    //        //            rightKnobMode,
    //        //                     buttonPressCount);
    //        //
    //        //            // If we are coming from the pattern selection,
    //        apply the
    //        //            new
    //        //            // pattern upon switching out of it. This is to
    //        prevent
    //        //            sudden
    //        //            // jarring pattern changes while scrolling through
    //        them
    //        //            "live". if (rightKnobMode == MODE_PATTERN) {
    //        //                Stroker.setPattern(int(strokePattern),
    //        //                                   false);  // Pattern, index
    //        must be
    //        //                                   <
    //        //                                            //
    //        // Stroker.getNumberOfPattern()
    //        //            }
    //        //
    //        //            if (buttonPressCount > 1)  // Enter
    //        pattern-selection mode
    //        //            if the
    //        //                                       // button is pressed more
    //        than
    //        //                                       once
    //        //            {
    //        //                rightKnobMode = MODE_PATTERN;
    //        //            } else if (strokePattern ==
    //        //                       0)  // If the button was only pressed
    //        once and
    //        //                       we're
    //        //                           // in the basic stroke engine
    //        pattern...
    //        //            {
    //        //                // ..clamp the right knob mode so that we bypass
    //        the
    //        //                // "sensation" mode
    //        //                rightKnobMode += 1;
    //        //                if (rightKnobMode > MODE_DEPTH) {
    //        //                    rightKnobMode = MODE_STROKE;
    //        //                }
    //        //            } else {
    //        //                // Otherwise allow us to select the sensation
    //        control
    //        //                mode rightKnobMode += 1; if (rightKnobMode >
    //        //                MODE_SENSATION) {
    //        //                    rightKnobMode = MODE_STROKE;
    //        //                }
    //        //            }
    //        //
    //        //            ESP_LOGD("UTILS", "switching mode: %i",
    //        rightKnobMode);
    //        //
    //        //            modeChanged = true;
    //        //            lastEncoderButtonPresses = encoderButtonPresses;
    //        //        }
    //
    //        if (lastStrokePercentage != strokePercentage) {
    //            float newStroke = 0.01f * strokePercentage *
    //            abs(measuredStrokeMm); ESP_LOGD("UTILS", "change stroke: %f
    //            %f", strokePercentage,
    //                     newStroke);
    //            Stroker.setStroke(newStroke, true);
    //            lastStrokePercentage = strokePercentage;
    //        }
    //
    //        if (lastDepthPercentage != depthPercentage) {
    //            float newDepth = 0.01f * depthPercentage *
    //            abs(measuredStrokeMm); ESP_LOGD("UTILS", "change depth: %f
    //            %f", depthPercentage, newDepth); Stroker.setDepth(newDepth,
    //            false); lastDepthPercentage = depthPercentage;
    //        }
    //
    //        if (lastSensationPercentage != sensationPercentage) {
    //            float newSensation = calculateSensation(sensationPercentage);
    //            ESP_LOGD("UTILS", "change sensation: %f %f",
    //            sensationPercentage,
    //                     newSensation);
    //            Stroker.setSensation(newSensation, false);
    //            lastSensationPercentage = sensationPercentage;
    //        }
    //
    //        //        if (!modeChanged && changePattern != 0) {
    //        //            strokePattern += changePattern;
    //        //
    //        //            if (strokePattern < 0) {
    //        //                strokePattern = Stroker.getNumberOfPattern() -
    //        1;
    //        //            } else if (strokePattern >=
    //        Stroker.getNumberOfPattern())
    //        //            {
    //        //                strokePattern = 0;
    //        //            }
    //        //
    //        //            ESP_LOGD("UTILS", "change pattern: %i",
    //        strokePattern);
    //        //
    //        //            strokerPatternName = Stroker.getPatternName(
    //        //                strokePattern);  // Update the stroke pattern
    //        name
    //        //                (used by
    //        //                                 // the UI)
    //        //
    //        //            modeChanged = true;
    //        //        }
    //
    //        vTaskDelay(400);
    //    }
}