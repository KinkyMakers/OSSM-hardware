#include "mqtt.h"

#include <Arduino.h>
#include <esp_log.h>
#include <utils/getEfuseMac.h>
#include <utils/random.h>

// Define the global variables here
bool mqttConnected = false;
esp_mqtt_client_handle_t mqttClient = nullptr;
String sessionId = "";

// MQTT server configuration
#if defined(VERSIONDEV) && defined(MQTT_SERVER)
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = 1883;
#else
const char* mqtt_server = "mqtts://c071b760.ala.us-east-1.emqxsl.com";
const int mqtt_port = 8883;
#endif

// certificate
#ifdef VERSIONDEV
static const char* root_ca = nullptr;
#else
static const char* root_ca =
    "-----BEGIN "
    "CERTIFICATE-----"
    "\nMIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh\nMQswCQ"
    "YDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\nd3cuZGlnaWNlcn"
    "QuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD\nQTAeFw0wNjExMTAwMDAwMD"
    "BaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT\nMRUwEwYDVQQKEwxEaWdpQ2VydCBJbm"
    "MxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\nb20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbC"
    "BSb290IENBMIIBIjANBgkqhkiG\n9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUK"
    "KPC3eQyaKl7hLOllsB\nCSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/"
    "fzTtxRuLWZscFs3YnFo97\nnh6Vfe63SKMI2tavegw5BmV/"
    "Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt\n43C/dxC//"
    "AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+"
    "hqkpMfT7P\nT19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/"
    "4\ngdW7jVg/"
    "tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+"
    "jkMOvJwIDAQABo2MwYTAO\nBgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/"
    "zAdBgNVHQ4EFgQUA95QNVbR\nTLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8"
    "KPiGxvDl7I90VUw\nDQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+"
    "t1EnE9SsPTfrgT1eXkIoyQY/Esr\nhMAtudXH/"
    "vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg\n06O/"
    "nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+"
    "0tKIJF\nPnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+"
    "Krk2U886UAb3LujEV0ls\nYSEY1QSteDwsOoBrp+"
    "uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk\nCAUw7C29C79Fv1C5qfPrmAESrc"
    "iIxpg0X40KPMbp1ZWVbd4=\n-----END CERTIFICATE-----";
#endif

void event_connected_handler(void* handler_args, esp_event_base_t base,
                             int32_t event_id, void* event_data) {
    ESP_LOGD("MQTT", "Connected to MQTT broker");
    mqttConnected = true;
}

void event_disconnected_handler(void* handler_args, esp_event_base_t base,
                                int32_t event_id, void* event_data) {
    ESP_LOGD("MQTT", "Disconnected from MQTT broker");
    mqttConnected = false;
}

void initMQTT() {
    delay(2000);

    ESP_LOGD("MQTT", "Initializing MQTT");

    String lwt = "{\"state\": \"disconnected\"}";
    String macAddress = getMacAddress();

    // this will be used to identify sessions to the
    sessionId = uuid();

    // Create new config with updated credentials
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = mqtt_server,
        .port = mqtt_port,
        .client_id = macAddress.c_str(),
        .username = "TEST",
        .password = "Hello123!",
        .lwt_topic = String("ossm/" + macAddress).c_str(),
        .lwt_msg = lwt.c_str(),
        .lwt_qos = 2,
        .lwt_retain = true,
        .keepalive = 15,
        .buffer_size = 1024,
        // .cert_pem = root_ca,
    };

    // Initialize and start new client with updated credentials
    ESP_LOGD("MQTT", "Initializing MQTT client");
    mqttClient = esp_mqtt_client_init(&mqtt_cfg);
    ESP_LOGD("MQTT", "Starting MQTT client");
    esp_mqtt_client_start(mqttClient);
    ESP_LOGD("MQTT", "MQTT client started");
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_CONNECTED,
                                   event_connected_handler, nullptr);
    esp_mqtt_client_register_event(mqttClient, MQTT_EVENT_DISCONNECTED,
                                   event_disconnected_handler, nullptr);
}
