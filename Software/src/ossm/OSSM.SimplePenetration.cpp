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
            Config::Driver::maxSpeedMmPerSecond * ossm->speedPercentage / 100.0;
        auto acceleration = Config::Driver::maxSpeedMmPerSecond *
                            ossm->speedPercentage * ossm->speedPercentage /
                            Config::Advanced::accelerationScaling;

        // If the speed is greater than the dead-zone, and the speed has changed
        // by more than the dead-zone, then update the stepper.
        // This must be done in the same task that the stepper is running in.
        if (ossm->speedPercentage >
                Config::Advanced::commandDeadZonePercentage &&
            abs(speed - lastSpeed) >
                Config::Advanced::commandDeadZonePercentage) {
            lastSpeed = speed;

            ossm->stepper.setSpeedInMillimetersPerSecond(speed);
            ossm->stepper.setAccelerationInMillimetersPerSecondPerSecond(
                acceleration);
            ossm->stepper.setDecelerationInMillimetersPerSecondPerSecond(
                acceleration);
        }

        if (ossm->stepper.getDistanceToTargetSigned() != 0) {
            vTaskDelay(1);
            // more than zero
            continue;
        }

        ossm->isForward = !ossm->isForward;

        if (ossm->speedPercentage >
                Config::Advanced::commandDeadZonePercentage &&
            ossm->strokePercentage >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            ossm->sessionStrokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            ossm->sessionDistanceMeters +=
                (0.002 * ((float)ossm->strokePercentage / 100.0) *
                 ossm->measuredStrokeMm);
        }

        double targetPosition =
            ossm->isForward ? -abs(((float)ossm->strokePercentage / 100.0) *
                                   ossm->measuredStrokeMm)
                            : 0;

        if (ossm->speedPercentage <
            Config::Advanced::commandDeadZonePercentage) {
            targetPosition = 0;
        }

        ESP_LOGD("SimplePenetration", "Moving stepper to position %d",
                 static_cast<long int>(targetPosition));

        ossm->stepper.setTargetPositionInMillimeters(targetPosition);

        // TODO: update life states.
        //        updateLifeStats();
        // Delay task for 1/20 seconds
        vTaskDelay(80);
    }
    vTaskDelete(nullptr);
}

void OSSM::startSimplePenetration() {
    int stackSize = 15 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startSimplePenetrationTask,
                            "startSimplePenetrationTask", stackSize, this,
                            configMAX_PRIORITIES - 1,
                            &runSimplePenetrationTaskH, operationTaskCore);
}