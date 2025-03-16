#include "Arduino.h"
#include "OneButton.h"
#include "WiFi.h"
#include "ossm/Events.h"
#include "ossm/OSSM.h"
#include "services/board.h"
#include "services/display.h"
#include "services/encoder.h"
#include "services/espnow.h"
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

/*
  Rui Santos
  Complete project details at
  https://RandomNerdTutorials.com/esp-now-esp32-arduino-ide/

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

#include <WiFi.h>
#include <esp_now.h>

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
    uint8_t speed;
    uint8_t stroke;
    uint8_t sens;
    uint8_t depth;
    uint8_t pattern;
} struct_message;

// Create a struct_message called myData
struct_message myData;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    memcpy(&myData, incomingData, sizeof(myData));
    Serial.print("Bytes received: ");
    Serial.println(len);
    Serial.print("speed: ");
    Serial.println(myData.speed);
    Serial.print("stroke: ");
    Serial.println(myData.stroke);
    Serial.print("sens: ");
    Serial.println(myData.sens);
    Serial.print("depth: ");
    Serial.println(myData.depth);
    Serial.print("pattern: ");
    Serial.println(myData.pattern);
    Serial.println();

    if (ossm == nullptr) {
        return;
    }

    switch (ossm->playControl) {
        case PlayControls::STROKE:
            ossm->encoder.setEncoderValue(myData.stroke);
            break;
        case PlayControls::DEPTH:
            ossm->encoder.setEncoderValue(myData.depth);
            break;
        case PlayControls::SENSATION:
            ossm->encoder.setEncoderValue(myData.sens);
            break;
    }

    ossm->setting.speed = myData.speed;
    ossm->setting.stroke = myData.stroke;
    ossm->setting.sensation = myData.sens;
    ossm->setting.depth = myData.depth;
    ossm->setting.pattern = static_cast<StrokePatterns>(myData.pattern);
}

void setup() {
    /** Board setup */
    initBoard();

    /** Service setup */
    // Encoder
    initEncoder();
    // Display
    display.setBusClock(400000);
    display.begin();

    delay(1000);
    ESP_LOGI("ESPNow", "Initializing ESP-NOW");
    // Initialize ESP-NOW for message listening
    // Set device as a Wi-Fi Station
    WiFi.mode(WIFI_STA);

    // Init ESP-NOW
    if (esp_now_init() != ESP_OK) {
        Serial.println("Error initializing ESP-NOW");
        return;
    }

    // Once ESPNow is successfully Init, we will register for recv CB to
    // get recv packer info
    esp_now_register_recv_cb(OnDataRecv);

    ossm = new OSSM(display, encoder, stepper);

    // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });
};

void loop() {
    button.tick();
    // ossm->wm.process();
};