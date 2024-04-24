#include "Arduino.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/stepper.h"
#include "state/events.h"
#include "state/globalState.h"
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

// TODO: Move this to a service
bool handlePress = false;
int counter = 0;
bool isDouble = false;
long lastPressed = 0;

void IRAM_ATTR encoderPressed() { handlePress = true; }

void setup() {
    initBoard();
    initEncoder();
    initStepper();
    display.begin();

    attachInterrupt(digitalPinToInterrupt(Pins::Remote::encoderSwitch),
                    encoderPressed, RISING);
    stateMachine.visit_current_states([](auto state) {
        ESP_LOGD("Homing", "Current state A: %s", state.c_str());
    });
    stateMachine.process_event(Done{});
    stateMachine.visit_current_states([](auto state) {
        ESP_LOGD("Homing", "Current state D: %s", state.c_str());
    });
};

void loop() {
    // TODO: Relocate this code.
    if (handlePress) {
        handlePress = false;

        // detect if a double click occurred
        if (millis() - lastPressed < 300) {
            stateMachine.process_event(DoublePress{});
        } else {
            stateMachine.process_event(ButtonPress{});
        }
        lastPressed = millis();

        ESP_LOGD("Loop", "%sButton Press", isDouble ? "Double " : "");
    }
};