#include "OSSM.h"

#include "services/stepper.h"

void startStreamingTask(void *pvParameters) {
    // OSSM *ossm = (OSSM *)pvParameters;

    // auto isInCorrectState = [](OSSM *ossm) {
    //     // Add any states that you want to support here.
    //     // return ossm->sm->is("streaming"_s) ||
    //     //        ossm->sm->is("streaming.preflight"_s) ||
    //     //        ossm->sm->is("streaming.idle"_s);
    //     return true;
    // };

    // // create a function that, given a time, returns a value between 0 and
    // 100
    // // from a sine wave with period of 1000ms
    // auto sineWave = [](int time) { return sin(time * 2 * M_PI / 1000) * 100;
    // };

    // while (isInCorrectState(ossm)) {
    //     stepper->moveTo(sineWave(millis()), false);
    //     vTaskDelay(1);
    // }

    // vTaskDelete(nullptr);
}

void OSSM::startStreaming() {
    // int stackSize = 10 * configMINIMAL_STACK_SIZE;

    // xTaskCreatePinnedToCore(startStreamingTask, "startStreamingTask",
    // stackSize,
    //                         this, configMAX_PRIORITIES - 1, nullptr,
    //                         Tasks::operationTaskCore);
}