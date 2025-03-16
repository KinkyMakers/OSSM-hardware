#include "espnow.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "esp_crc.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_random.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"

#define ESP_NOW_TAG "ESP-NOW"

#define ESPNOW_MAXDELAY 512

static const char *TAG = "espnow_example";

static QueueHandle_t s_example_espnow_queue = NULL;

static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
                                                            0xFF, 0xFF, 0xFF};
static uint16_t s_example_espnow_seq[EXAMPLE_ESPNOW_DATA_MAX] = {0, 0};

// Parse received message
StrokeMessage parseStrokeMessage(const String &message) {
    StrokeMessage result;

    // Initialize with default values
    result.strokeType = "";
    result.speed = 0;
    result.stroke = 0;
    result.strokeRate = 0;
    result.strokeNumber = 0;
    result.depth = 0;
    result.pattern = 0;

    // Split the message by commas
    int startPos = 0;
    int endPos = message.indexOf(',');

    while (endPos != -1 || startPos < message.length()) {
        String part;
        if (endPos != -1) {
            part = message.substring(startPos, endPos);
            startPos = endPos + 1;
        } else {
            part = message.substring(startPos);
            startPos = message.length();
        }

        // Parse each part (format: XX:value)
        int colonPos = part.indexOf(':');
        if (colonPos != -1) {
            String key = part.substring(0, colonPos);
            String value = part.substring(colonPos + 1);

            if (key == "ST") {
                result.strokeType = value;
            } else if (key == "SP") {
                result.speed = value.toInt();
            } else if (key == "SK") {
                result.stroke = value.toInt();
            } else if (key == "SR") {
                result.strokeRate = value.toInt();
            } else if (key == "SN") {
                result.strokeNumber = value.toInt();
            } else if (key == "DP") {
                result.depth = value.toInt();
            } else if (key == "PT") {
                result.pattern = value.toInt();
            }
        }

        endPos = message.indexOf(',', startPos);
    }

    return result;
}

// Callback function for when data is received
void onDataReceived(const uint8_t *mac, const uint8_t *data, int len) {
    char macStr[18];
    snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x", mac[0],
             mac[1], mac[2], mac[3], mac[4], mac[5]);

    // Convert received data to string
    char *message = (char *)malloc(len + 1);
    if (message == NULL) {
        ESP_LOGD(ESP_NOW_TAG, "Memory allocation failed");
        return;
    }

    memcpy(message, data, len);
    message[len] = '\0';

    ESP_LOGD(ESP_NOW_TAG, "Received from: %s", macStr);
    ESP_LOGD(ESP_NOW_TAG, "Message: %s", message);

    // Parse the message
    String messageStr(message);
    StrokeMessage strokeMsg = parseStrokeMessage(messageStr);

    // Log the parsed message
    ESP_LOGD(ESP_NOW_TAG, "Parsed Message:");
    ESP_LOGD(ESP_NOW_TAG, "  Stroke Type: %s", strokeMsg.strokeType.c_str());
    ESP_LOGD(ESP_NOW_TAG, "  Speed: %d", strokeMsg.speed);
    ESP_LOGD(ESP_NOW_TAG, "  Stroke: %d", strokeMsg.stroke);
    ESP_LOGD(ESP_NOW_TAG, "  Stroke Rate: %d", strokeMsg.strokeRate);
    ESP_LOGD(ESP_NOW_TAG, "  Stroke Number: %d", strokeMsg.strokeNumber);
    ESP_LOGD(ESP_NOW_TAG, "  Depth: %d", strokeMsg.depth);
    ESP_LOGD(ESP_NOW_TAG, "  Pattern: %d", strokeMsg.pattern);

    free(message);
}

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA

/* WiFi should start before using ESPNOW */
static void example_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static esp_err_t example_espnow_init(void) {
    /* Initialize ESPNOW and register sending and receiving callback function.
     */
    ESP_ERROR_CHECK(esp_now_init());
    // ESP_ERROR_CHECK(esp_now_register_send_cb(example_espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(onDataReceived));

    /* Set primary master key. */
    // ESP_ERROR_CHECK(esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK));

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = (esp_now_peer_info_t *)malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(ESP_NOW_TAG, "Malloc peer information fail");
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    // peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = static_cast<wifi_interface_t>(ESPNOW_WIFI_IF);
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer));
    free(peer);

    return ESP_OK;
}

// Initialize ESP-NOW
void initEspNow() {
    example_wifi_init();
    example_espnow_init();

    ESP_LOGD(ESP_NOW_TAG, "ESP-NOW initialized and listening for messages");
}
