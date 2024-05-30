#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"

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

OSSM *ossm;

OneButton button(Pins::Remote::encoderSwitch, false);

void setup() {
    /** Board setup */
    initBoard();

    /** Service setup */
    // Encoder
    initEncoder();
    // Display
    display.setBusClock(400000);
    display.begin();

    ossm = new OSSM(display, encoder, stepper);
    // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });

    xTaskCreate(
        [](void* pvParameters) {
            while (true) {
                button.tick();
                ossm->wm.process();
                vTaskDelay(10);
            }
        },
        "buttonTask", 4 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
};

void loop() { vTaskDelete(nullptr); };