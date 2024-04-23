#include "Arduino.h"
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

// TODO: Move this to a service
bool handlePress = false;
int counter = 0;
bool isDouble = false;
long lastPressed = 0;

void IRAM_ATTR encoderPressed() { handlePress = true; }

void setup() {
    /** Board setup */
    initBoard();

    /** Service setup */
    // Encoder
    initEncoder();
    // Display
    display.begin();

    ossm = new OSSM(display, encoder);

    attachInterrupt(digitalPinToInterrupt(Pins::Remote::encoderSwitch),
                    encoderPressed, RISING);
};

void loop() {
    // TODO: Relocate this code.
    if (handlePress) {
        handlePress = false;

        // detect if a double click occurred
        if (millis() - lastPressed < 300) {
            ossm->sm->process_event(DoublePress{});
        } else {
            ossm->sm->process_event(ButtonPress{});
        }
        lastPressed = millis();

        ESP_LOGD("Loop", "%sButton Press", isDouble ? "Double " : "");
    }
};