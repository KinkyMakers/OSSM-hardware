#include "stroke_engine.h"

#include "ossm/state/ble.h"
#include "ossm/state/calibration.h"
#include "ossm/state/settings.h"
#include "ossm/state/state.h"
#include "services/stepper.h"
#include "services/tasks.h"
#include "structs/SettingPercents.h"
#include "utils/StrokeEngineHelper.h"

namespace sml = boost::sml;
using namespace sml;

namespace stroke_engine {

static void startStrokeEngineTask(void *pvParameters) {
    float measuredStrokeMm = calibration.measuredStrokeSteps / (1_mm);

    machineGeometry strokingMachine = {
        .physicalTravel = abs(calibration.measuredStrokeSteps / (1_mm)),
        .keepoutBoundary = 6.0};
    SettingPercents lastSetting = settings;

    Stroker.begin(&strokingMachine, &servoMotor, stepper);
    Stroker.thisIsHome();

    Stroker.setSensation(calculateSensation(settings.sensation), true);

    Stroker.setDepth(0.01f * settings.depth * abs(measuredStrokeMm), true);
    Stroker.setStroke(0.01f * settings.stroke * abs(measuredStrokeMm), true);

    auto isInCorrectState = []() {
        // Add any states that you want to support here.
        return stateMachine->is("strokeEngine"_s) ||
               stateMachine->is("strokeEngine.idle"_s) ||
               stateMachine->is("strokeEngine.pattern"_s);
    };

    while (isInCorrectState()) {
        if (isChangeSignificant(lastSetting.speed, settings.speed) ||
            wasLastSpeedCommandFromBLE(true)) {
            // Speed is float, so give a little wiggle room here to assume 0
            if (settings.speed < 0.1f) {
                Stroker.stopMotion();
            } else if (Stroker.getState() == READY) {
                Stroker.startPattern();
            }

            Stroker.setSpeed(settings.speed * 3, true);
            lastSetting.speed = settings.speed;
        }

        if (lastSetting.stroke != settings.stroke) {
            float newStroke = 0.01f * settings.stroke * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change stroke: %f %f", settings.stroke,
                     newStroke);
            Stroker.setStroke(newStroke, true);
            lastSetting.stroke = settings.stroke;
        }

        if (lastSetting.depth != settings.depth) {
            float newDepth = 0.01f * settings.depth * abs(measuredStrokeMm);
            ESP_LOGD("UTILS", "change depth: %f %f", settings.depth, newDepth);
            Stroker.setDepth(newDepth, false);
            lastSetting.depth = settings.depth;
        }

        if (lastSetting.sensation != settings.sensation) {
            float newSensation = calculateSensation(settings.sensation);
            ESP_LOGD("UTILS", "change sensation: %f %f", settings.sensation,
                     newSensation);
            Stroker.setSensation(newSensation, false);
            lastSetting.sensation = settings.sensation;
        }

        if (lastSetting.pattern != settings.pattern) {
            ESP_LOGD("UTILS", "change pattern: %d", settings.pattern);

            switch (settings.pattern) {
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

            lastSetting.pattern = settings.pattern;
        }

        if (bleState.hasActiveConnection) {
            // When connected to BLE, update more frequently for improved
            // responsiveness
            vTaskDelay(100);
        } else {
            vTaskDelay(400);
        }
    }

    Stroker.stopMotion();

    vTaskDelete(nullptr);
}

void startStrokeEngine() {
    int stackSize = 12 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startStrokeEngineTask, "startStrokeEngineTask",
                            stackSize, nullptr, configMAX_PRIORITIES - 1,
                            &Tasks::runStrokeEngineTaskH,
                            Tasks::operationTaskCore);
}

}  // namespace stroke_engine
