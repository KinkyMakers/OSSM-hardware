#include "OSSM.h"

#include "constants/Config.h"

void OSSM::startSimplePenetrationTask(void *pvParameters) {
    OSSM *ossm = (OSSM *)pvParameters;

    int fullStrokeCount = 0;

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("simplePenetration"_s) ||
               ossm->sm->is("simplePenetration.idle"_s);
    };

    double lastSpeed = 0;

    while (isInCorrectState(ossm)) {
        auto speed =
            Config::Driver::maxSpeedMmPerSecond * ossm->setting.speed / 100.0;
        auto acceleration = Config::Driver::maxSpeedMmPerSecond *
                            ossm->setting.speed * ossm->setting.speed /
                            Config::Advanced::accelerationScaling;

        bool isSpeedZero = ossm->setting.speedKnob <
                           Config::Advanced::commandDeadZonePercentage;
        bool isSpeedChanged =
            !isSpeedZero && abs(speed - lastSpeed) >
                                5 * Config::Advanced::commandDeadZonePercentage;
        bool isAtTarget = ossm->stepper.getDistanceToTargetSigned() == 0;

        // If the speed is zero, then stop the stepper and wait for the next
        if (isSpeedZero) {
            ossm->stepper.emergencyStop();
            vTaskDelay(50);
            continue;
        }

        // If the speed is greater than the dead-zone, and the speed has changed
        // by more than the dead-zone, then update the stepper.
        // This must be done in the same task that the stepper is running in.
        if (isSpeedChanged) {
            lastSpeed = speed;

            ossm->stepper.setSpeedInMillimetersPerSecond(speed);
            ossm->stepper.setAccelerationInMillimetersPerSecondPerSecond(
                acceleration);
            ossm->stepper.setDecelerationInMillimetersPerSecondPerSecond(
                acceleration);
        }

        // If the stepper is not at the target, then wait for the next loop
        if (!isAtTarget) {
            vTaskDelay(1);
            // more than zero
            continue;
        }

        bool nextDirection = !ossm->isForward;
        ossm->isForward = nextDirection;

        double targetPosition =
            ossm->isForward ? -abs(((float)ossm->setting.stroke / 100.0) *
                                   ossm->measuredStrokeMm)
                            : 0;

        ESP_LOGV("SimplePenetration", "target: %f,\tspeed: %f,\tacc: %f",
                 targetPosition, speed, acceleration);

        ossm->stepper.setTargetPositionInMillimeters(targetPosition);

        if (ossm->setting.speed > Config::Advanced::commandDeadZonePercentage &&
            ossm->setting.stroke >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            ossm->sessionStrokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            ossm->sessionDistanceMeters +=
                (0.002 * ((float)ossm->setting.stroke / 100.0) *
                 ossm->measuredStrokeMm);
        }

        vTaskDelay(1);
    }
    vTaskDelete(nullptr);
}

void OSSM::startSimplePenetration() {
    int stackSize = 30 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startSimplePenetrationTask,
                            "startSimplePenetrationTask", stackSize, this,
                            configMAX_PRIORITIES - 1,
                            &runSimplePenetrationTaskH, operationTaskCore);
}