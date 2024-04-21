#include "OSSM.h"

#include <DebugLog.h>

#include "Events.h"
#include "constants/UserConfig.h"
#include "services/stepper.h"
#include "utilities/analog.h"

namespace sml = boost::sml;
using namespace sml;

/** OSSM Homing methods
 *
 * This is a collection of methods that are associated with the homing state on
 * the OSSM. technically anything can go here, but please try to keep it
 * organized.
 */
void OSSM::clearHoming() {
    LOG_TRACE("HomePage::homingStart");
    isForward = true;

    // Drop the speed and acceleration to something reasonable.
    stepper.setAccelerationInMillimetersPerSecondPerSecond(1000);
    stepper.setDecelerationInMillimetersPerSecondPerSecond(10000);
    stepper.setSpeedInMillimetersPerSecond(25);

    // Clear the stored values.
    this->measuredStrokeMm = 0;

    // Recalibrate the current sensor offset.
    this->currentSensorOffset = (getAnalogAveragePercent(
        SampleOnPin{Pins::Driver::currentSensorPin, 1000}));
};

void OSSM::startHomingTask(void *pvParameters) {
    LOG_TRACE("OSSM::startHomingTask :D");
    TickType_t xTaskStartTime = xTaskGetTickCount();

    // parse parameters to get OSSM reference
    OSSM *ossm = (OSSM *)pvParameters;

    float target = ossm->isForward ? -400 : 400;
    ossm->stepper.setTargetPositionInMillimeters(target);

    auto isInCorrectState = [](OSSM *ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("homing"_s) || ossm->sm->is("homing.idle"_s) ||
               ossm->sm->is("homing.backward"_s);
    };
    // run loop for 15second or until loop exits
    while (isInCorrectState(ossm)) {
        TickType_t xCurrentTickCount = xTaskGetTickCount();
        // Calculate the time in ticks that the task has been running.
        TickType_t xTicksPassed = xCurrentTickCount - xTaskStartTime;

        // If you need the time in milliseconds, convert ticks to milliseconds.
        // 'portTICK_PERIOD_MS' is the number of milliseconds per tick.
        uint32_t msPassed = xTicksPassed * portTICK_PERIOD_MS;

        if (msPassed > 15000) {
            LOG_ERROR(
                "HomePage::homing, homing took too long. Check power and "
                "restart.");
            ossm->errorMessage = UserConfig::language.HomingTookTooLong;
            ossm->sm->process_event(Error{});
            break;
        }

        // measure the current analog value.
        float current = getAnalogAveragePercent(
                            SampleOnPin{Pins::Driver::currentSensorPin, 200}) -
                        ossm->currentSensorOffset;

        LOG_TRACE("Homing current: " + String(current));

        // If we have not detected a "bump" with a hard stop, then return and
        // let the loop continue.
        if (current < Config::Driver::sensorlessCurrentLimit &&
            ossm->stepper.getCurrentPositionInMillimeters() <
                Config::Driver::maxStrokeLengthMm) {
            // Saying hi to the watchdog :).
            vTaskDelay(1);
            continue;
        }

        LOG_DEBUG("Homing Bump Detected!");

        // Otherwise, if we have detected a bump, then we need to stop the
        // motor.
        ossm->stepper.setTargetPositionToStop();

        // And then move the motor back by the configured offset.
        // This offset will be positive for reverse homing and negative for
        // forward homing.
        float sign = ossm->isForward ? 1.0f : -1.0f;
        ossm->stepper.moveRelativeInMillimeters(
            sign *
            Config::Advanced::strokeZeroOffsetMm);  //"move to" is blocking

        if (!ossm->isForward) {
            // If we are homing backward, then we need to measure the stroke
            // length before resetting the home position.
            ossm->measuredStrokeMm =
                min(abs(ossm->stepper.getCurrentPositionInMillimeters()),
                    Config::Driver::maxStrokeLengthMm);

            LOG_DEBUG("HomePage::homing, measuredStrokeMm: " +
                      String(ossm->measuredStrokeMm));
        }

        // And finally, we'll set the most forward position as the new "zero"
        // position.
        ossm->stepper.setCurrentPositionAsHomeAndStop();

        // Set the event to done so that the machine will move to the next
        // state.
        ossm->sm->process_event(Done{});
        break;
    };

    vTaskDelete(nullptr);
}

void OSSM::startHoming() {
    // Create task
    xTaskCreatePinnedToCore(startHomingTask, "startHomingTask", 10000, this, 1,
                            &operationTask, 0);
}

auto OSSM::isStrokeTooShort() -> bool {
    if (measuredStrokeMm > Config::Driver::minStrokeLengthMm) {
        return false;
    }
    this->errorMessage = UserConfig::language.StrokeTooShort;
    return true;
}
