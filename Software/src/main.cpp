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
#include "services/mqtt.hpp"
#include "utils/dashboard.hpp"

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

// MQTT Broker settings
//      const char* mqtt_broker =
//      "jf90d32f.ala.dedicated.aws.emqxcloud.com";
// localhost broker
const char* mqtt_broker = "0.0.0.0";
const char* mqtt_username = "test";       // Not always needed
const char* mqtt_password = "Hello123!";  // Not always needed
const int mqtt_port = 1883;

// WiFi credentials
const char* ssid = "Treehouse";
const char* password = "lilbabysugar";
String sessionId = "";

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

    AuthResponse token = getAuthToken();
    sessionId = token.sessionId;
    // Connect to MQTT
    client.setServer(mqtt_broker, mqtt_port);
    while (!client.connected()) {
        Serial.println("Connecting to MQTT...");
        if (client.connect(token.clientId.c_str(), token.username.c_str(),
                           token.password.c_str())) {
            ESP_LOGD("MQTT", "Connected to MQTT");
        } else {
            ESP_LOGD("MQTT", "Failed with state: %s", clientState().c_str());
            delay(5000);
        }
    }
}

void setup() {
    Serial.begin(115200);

    /** Board setup */
    initBoard();
    setup_wifi();

    /** Service setup */
    // start client connection task in the background
    xTaskCreatePinnedToCore(
        [](void* pvParameters) {
            int i = 0;
            static StaticJsonDocument<200> doc;

            vTaskDelay(5000 / portTICK_PERIOD_MS);
            while (true) {
                // if WIFI not connected then wait
                if (WiFi.status() != WL_CONNECTED || !client.connected()) {
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                    continue;
                }

                i++;
                client.loop();

                doc["sessionId"] = sessionId;
                doc["x"] = random(0, 100);
                doc["ms"] = millis();

                String output;
                serializeJson(doc, output);

                // SERIAL LOG VALUE
                ESP_LOGD("MQTT", "Sending: %s", output.c_str());
                client.publish("ossm/test", output.c_str());

                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        },
        "mqtt", 3 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr, stepperCore);

    ossm = new OSSM(display, encoder);
    // link functions to be called on events.
    button.attachClick([]() { ossm->sm->process_event(ButtonPress{}); });
    button.attachDoubleClick([]() { ossm->sm->process_event(DoublePress{}); });
    button.attachLongPressStart([]() { ossm->sm->process_event(LongPress{}); });
};

void loop() { button.tick(); };