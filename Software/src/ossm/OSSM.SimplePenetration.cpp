#include "OSSM.h"

#include "ArduinoJson.h"
#include "PubSubClient.h"
#include "utils/dashboard.hpp"

void OSSM::startSimplePenetrationTask(void* pvParameters) {
    OSSM* ossm = (OSSM*)pvParameters;

    int fullStrokeCount = 0;
    int32_t targetPosition = 0;

    auto isInCorrectState = [](OSSM* ossm) {
        // Add any states that you want to support here.
        return ossm->sm->is("simplePenetration"_s) ||
               ossm->sm->is("simplePenetration.idle"_s);
    };

    double lastSpeed = 0;

    bool stopped = false;

    while (isInCorrectState(ossm)) {
        auto speed = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                     ossm->setting.speed / 100.0;
        auto acceleration = (1_mm) * Config::Driver::maxSpeedMmPerSecond *
                            ossm->setting.speed * ossm->setting.speed /
                            Config::Advanced::accelerationScaling;

        bool isSpeedZero = ossm->setting.speedKnob <
                           Config::Advanced::commandDeadZonePercentage;
        bool isSpeedChanged =
            !isSpeedZero && abs(speed - lastSpeed) >
                                5 * Config::Advanced::commandDeadZonePercentage;
        bool isAtTarget =
            abs(targetPosition - ossm->stepper->getCurrentPosition()) == 0;

        // If the speed is zero, then stop the stepper and wait for the next
        if (isSpeedZero) {
            ossm->stepper->stopMove();
            stopped = true;
            vTaskDelay(100);
            continue;
        } else if (stopped) {
            ossm->stepper->moveTo(targetPosition, false);
            stopped = false;
        }

        // If the speed is greater than the dead-zone, and the speed has changed
        // by more than the dead-zone, then update the stepper.
        // This must be done in the same task that the stepper is running in.
        if (isSpeedChanged) {
            lastSpeed = speed;
            ossm->stepper->setAcceleration(acceleration);
            ossm->stepper->setSpeedInHz(speed);
        }

        // If the stepper is not at the target, then wait for the next lo

        if (!isAtTarget) {
            vTaskDelay(1);
            // more than zero
            continue;
        }

        bool nextDirection = !ossm->isForward;
        ossm->isForward = nextDirection;

        if (ossm->isForward) {
            targetPosition = -abs(((float)ossm->setting.stroke / 100.0) *
                                  ossm->measuredStrokeSteps);
        } else {
            targetPosition = 0;
        }

        ESP_LOGV("SimplePenetration", "target: %f,\tspeed: %f,\tacc: %f",
                 targetPosition, speed, acceleration);

        ossm->stepper->moveTo(targetPosition, false);

        if (ossm->setting.speed > Config::Advanced::commandDeadZonePercentage &&
            ossm->setting.stroke >
                (long)Config::Advanced::commandDeadZonePercentage) {
            fullStrokeCount++;
            ossm->sessionStrokeCount = floor(fullStrokeCount / 2);

            // This calculation assumes that at the end of every stroke you have
            // a whole positive distance, equal to maximum target position.
            ossm->sessionDistanceMeters +=
                (((float)ossm->setting.stroke / 100.0) *
                 ossm->measuredStrokeSteps / (1_mm)) /
                1000.0;
        }

        vTaskDelay(1);
    }

    vTaskDelete(nullptr);
}

void OSSM::startSimplePenetration() {
    int stackSize = 10 * configMINIMAL_STACK_SIZE;

    xTaskCreatePinnedToCore(startSimplePenetrationTask,
                            "startSimplePenetrationTask", stackSize, this,
                            configMAX_PRIORITIES - 1,
                            &runSimplePenetrationTaskH, operationTaskCore);

    xTaskCreate(
        [](void* pvParameters) {
            AuthResponse token = getAuthToken();
            WiFiClient wiFiClient;
            PubSubClient client(wiFiClient);
            String sessionId = "";
            const char* mqtt_broker = MQTT_BROKER;
            const int mqtt_port = MQTT_PORT;

            sessionId = token.sessionId;
            // Connect to MQTT
            client.setServer(mqtt_broker, mqtt_port);
            // increase packet size
            client.setBufferSize(1024);
            while (!client.connected()) {
                Serial.println("Connecting to MQTT...");
                vTaskDelay(1000 / portTICK_PERIOD_MS);
                if (client.connect(token.clientId.c_str(),
                                   token.username.c_str(),
                                   token.password.c_str())) {
                    ESP_LOGD("MQTT", "Connected to MQTT");
                } else {
                    ESP_LOGD("MQTT", "Failed with state: %d", client.state());
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                }
            }

            int i = 0;
            StaticJsonDocument<200> doc;

            vTaskDelay(5000 / portTICK_PERIOD_MS);
            while (true) {
                // if WIFI not connected then wait
                if (WiFi.status() != WL_CONNECTED || !client.connected()) {
                    vTaskDelay(5000 / portTICK_PERIOD_MS);
                    continue;
                }

                i++;
                client.loop();

                doc["x"] = random(0, 100);
                doc["ms"] = millis();
                doc["meta"] = "{}";

                String output;
                serializeJson(doc, output);

                // SERIAL LOG VALUE
                ESP_LOGD("MQTT", "Sending: %s", output.c_str());
                client.publish(String("ossm/" + sessionId).c_str(),
                               output.c_str());

                vTaskDelay(5000 / portTICK_PERIOD_MS);
            }
        },
        "mqtt", 6 * configMINIMAL_STACK_SIZE, nullptr, 1, nullptr);
}