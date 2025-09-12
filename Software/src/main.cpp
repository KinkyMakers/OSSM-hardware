#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "components/HeaderBar.h"
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
    initDisplay();

    // Initialize header bar task
    initHeaderBar();

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

    // Initialize NimBLE only when in menu.idle state
    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            bool initialized = false;
            while (true) {
                if ((ossm->sm->is("menu.idle"_s) ||
                     ossm->sm->is("error.idle"_s)) &&
                    !initialized) {
                    ESP_LOGD("MAIN", "Initializing NimBLE");
                    initNimble();
                    initialized = true;
                    vTaskDelete(nullptr);
                }
                vTaskDelay(pdMS_TO_TICKS(100));
            }
        },
        "initNimbleTask", 6 * configMINIMAL_STACK_SIZE, nullptr,
        configMAX_PRIORITIES - 1, nullptr, 0);
};

void loop() { vTaskDelete(nullptr); };
