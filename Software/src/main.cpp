#include "Arduino.h"
#include "WiFi.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "utils/StrokeEngineHelper.h"

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
static int lastState = HIGH;
static unsigned long fallTime = millis();
static unsigned long riseTime = millis();
static bool handlePress = false;
static bool watchForLongPress = false;
static unsigned long lastPressed = millis();

void IRAM_ATTR handleEncoder() {
    int currentState = digitalRead(Pins::Remote::encoderSwitch);

    if (currentState == HIGH && lastState == LOW) {
        // Pressing down.
        riseTime = millis();
        fallTime = millis();
        watchForLongPress = true;
    } else if (currentState == LOW && lastState == HIGH) {
        // Releasing Press
        riseTime = millis();
        handlePress = true;
        watchForLongPress = false;
    }

    lastState = currentState;  // Update lastState to the new state
}

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
                    handleEncoder, CHANGE);
};

void loop() {
    // TODO: Relocate this code.

    // if the encoder is down and has been for 1000ms, don't wait for the rise,
    // just trigger a long press.
    if (watchForLongPress) {
        int currentState = digitalRead(Pins::Remote::encoderSwitch);
        if (currentState == HIGH && millis() - fallTime > 1000 &&
            millis() - lastPressed > 1000) {
            ossm->sm->process_event(LongPress{});
            fallTime = millis();
            lastPressed = millis();
        }
    }

    if (handlePress) {
        handlePress = false;
        unsigned long pressTime = riseTime - fallTime;
        ESP_LOGD("Encoder", "Press time: %d, %d", pressTime, millis() - lastPressed);

        // detect if a double click occurred
        if (millis() - lastPressed < 300) {
            ossm->sm->process_event(DoublePress{});
        } else {
            ossm->sm->process_event(ButtonPress{});
        }
        lastPressed = millis();
    }
};