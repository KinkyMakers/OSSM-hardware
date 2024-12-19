#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/stepper.h"

#ifdef OSSM_CURRENT_MEAS_INA219
#include <Wire.h>
#include <Adafruit_INA219.h>
#endif

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

OSSM* ossm;

OneButton button(Pins::Remote::encoderSwitch, false);

#ifdef OSSM_CURRENT_MEAS_INA219

TwoWire Wire_OSSM = TwoWire(0);

Adafruit_INA219 ina219(0x40);
#endif

void setup() {
    Wire_OSSM.setPins(2,1);
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
};

void loop() {
    button.tick();
    ossm->wm.process();
};