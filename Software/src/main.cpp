#include "Arduino.h"
#include "ArduinoJson.h"
#include "OneButton.h"
#include "PubSubClient.h"
#include "WiFi.h"
#include "esp_log.h"
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

OSSM* ossm;

OneButton button(Pins::Remote::encoderSwitch, false);

// WiFi credentials
const char* ssid = "Treehouse";
const char* password = "lilbabysugar";

void setup_wifi() {
    delay(10);
    // Connect to WiFi
    Serial.println("Connecting to WiFi..");
    WiFi.begin(ssid, password);
    while (WiFiClass::status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi Connected!");
}

void setup() {
    Serial.begin(115200);

    /** Board setup */
    initBoard();
    setup_wifi();

    /** Service setup */

    ossm = new OSSM(display, encoder, stepper);
    // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });

    xTaskCreate(
        [](void* pvParameters) {
            while (true) {
                button.tick();
                vTaskDelay(10);
            }
        },
        "buttonTask", 4 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
};

void loop() { vTaskDelete(nullptr); };