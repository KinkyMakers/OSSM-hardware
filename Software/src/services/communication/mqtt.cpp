#include "mqtt.h"

#include <Arduino.h>
#include <esp_log.h>
#include <utils/getEfuseMac.h>
#include <utils/random.h>

// Define the global variables here
bool mqttConnected = false;
esp_mqtt_client_handle_t mqttClient = nullptr;
String sessionId = "";

// certificate
#ifdef VERSIONDEV
static const char* root_ca = nullptr;
#else
static const char* root_ca =
    "-----BEGIN CERTIFICATE-----\n"
    "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
    "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
    "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
    "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
    "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
    "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
    "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
    "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
    "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
    "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
    "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
    "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
    "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
    "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
    "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
    "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
    "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
    "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
    "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
    "MrY=\n"
    "-----END CERTIFICATE-----";
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
    String lwtTopic = "ossm/" + macAddress;

    // this will be used to identify sessions to the
    sessionId = uuid();

    // Create new config with updated credentials
    esp_mqtt_client_config_t mqtt_cfg = {
        .uri = MQTT_SERVER,
        .port = MQTT_PORT,
        .client_id = macAddress.c_str(),
        .username = MQTT_USERNAME,
        .password = MQTT_PASSWORD,
        .lwt_topic = lwtTopic.c_str(),
        .lwt_msg = lwt.c_str(),
        .lwt_qos = 2,
        .lwt_retain = true,
        .keepalive = 15,
        .buffer_size = 1024,
        .cert_pem = root_ca};

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
