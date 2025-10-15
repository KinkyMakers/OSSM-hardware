#include "Arduino.h"
#include "OneButton.h"
#include "components/HeaderBar.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "ossm/OSSMI.h"
#include "services/board.h"
#include "services/communication/nimble.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/stepper.h"
#include "services/led.h"
#include "services/wm.h"

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
    yield(); // Feed watchdog

    ESP_LOGD("MAIN", "Starting OSSM");

    initWM();
    yield(); // Feed watchdog

    // Display
    initDisplay();
    yield(); // Feed watchdog

    // Initialize header bar task
    initHeaderBar();
    yield(); // Feed watchdog

    ossm = new OSSM(display, encoder, stepper);
    ossmInterface = ossm;
    yield(); // Feed watchdog

    // Initialize LED for BLE and machine status indication
    ESP_LOGI("MAIN", "LED initialized for BLE and machine status indication");
    updateLEDForMachineStatus();  // Set initial LED state
    yield(); // Feed watchdog

    // // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });

    xTaskCreatePinnedToCore(
        [](void *pvParameters) {
            while (true) {
                button.tick();
                vTaskDelay(25 / portTICK_PERIOD_MS);
            }
        },
        "buttonTask", 4 * configMINIMAL_STACK_SIZE, nullptr,
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
        
    ESP_LOGI("MAIN", "Setup completed successfully");
};

void loop() { vTaskDelete(nullptr); };
