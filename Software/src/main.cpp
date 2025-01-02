#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/OSSMI.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/nimble.h"
#include "services/stepper.h"

/*
 *  ██████╗ ███████╗███████╗███╗   ███╗
 * ██╔═══██╗██╔════╝██╔════╝████╗ ████║
 * ██║   ██║███████╗███████╗██╔████╔██║
 * ██║   ██║╚════██║╚════██║██║╚██╔╝██║
 * ╚██████╔╝███████║███████║██║ ╚═╝ ██║
 *  ╚═════╝ ╚══════╝╚══════╝╚═╝     ╚═╝
 *
 * Welcome to the open source sex machine!
 * This is a product of Kinky Makers and is licensed under the MIT license.
 *
 * Research and Desire is a financial sponsor of this project.
 *
 * But our biggest sponsor is you! If you want to support this project, please
 * contribute, fork, branch and share!
 */

OneButton button(Pins::Remote::encoderSwitch, false);

void setup() {
    /** Board setup */
    initBoard();

    ESP_LOGD("MAIN", "Starting OSSM");

    // // Display
    display.setBusClock(400000);
    display.begin();

    ossm = new OSSM(display, encoder, stepper);
    ossmInterface = ossm;

    // // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });

    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            while (true) {
                button.tick();
                // ossm->wm.process();
                vTaskDelay(25 / portTICK_PERIOD_MS);
            }
        },
        "buttonTask", 6 * configMINIMAL_STACK_SIZE, nullptr,
        configMAX_PRIORITIES - 1, nullptr, 0);

    // Initialize NimBLE last.
    initNimble();
};

void loop() { vTaskDelete(nullptr); };
