#include "http.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "esp_log.h"

namespace HttpService {

    // ─────────────────────────────────────────────
    // Configuration
    // ─────────────────────────────────────────────

    static const char* BASE_URL = "http://192.168.2.15:3000";
    static const int QUEUE_SIZE = 10;
    static const int HTTP_TIMEOUT_MS = 5000;
    static const int MAX_RETRIES = 2;

    // ─────────────────────────────────────────────
    // Data Structures
    // ─────────────────────────────────────────────

    struct HttpRequest {
        String endpoint;
        String payload;
    };

    // ─────────────────────────────────────────────
    // State Variables
    // ─────────────────────────────────────────────

    static QueueHandle_t requestQueue = nullptr;
    static TaskHandle_t httpTaskHandle = nullptr;
    static String macAddress = "";

    // ─────────────────────────────────────────────
    // Private Functions
    // ─────────────────────────────────────────────

    static bool sendPost(const String& endpoint, const String& payload) {
        if (WiFi.status() != WL_CONNECTED) {
            ESP_LOGW("HTTP", "WiFi not connected, skipping request");
            return false;
        }

        HTTPClient http;
        String url = String(BASE_URL) + endpoint;

        http.begin(url);
        http.setTimeout(HTTP_TIMEOUT_MS);
        http.addHeader("Content-Type", "application/json");

        int httpCode = http.POST(payload);

        if (httpCode > 0) {
            ESP_LOGI("HTTP", "POST %s → %d", endpoint.c_str(), httpCode);

            if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_CREATED ||
                httpCode == HTTP_CODE_NO_CONTENT) {
                http.end();
                return true;
            }
        } else {
            ESP_LOGE("HTTP", "POST failed: %s",
                     http.errorToString(httpCode).c_str());
        }

        http.end();
        return false;
    }

    static void httpTask(void* pvParameters) {
        ESP_LOGI("HTTP", "HTTP task started on Core %d", xPortGetCoreID());

        HttpRequest request;

        while (true) {
            // Block until a request is available
            if (xQueueReceive(requestQueue, &request, portMAX_DELAY) ==
                pdTRUE) {
                // Retry logic
                bool success = false;
                for (int attempt = 0; attempt < MAX_RETRIES && !success;
                     attempt++) {
                    if (attempt > 0) {
                        ESP_LOGW("HTTP", "Retry attempt %d", attempt + 1);
                        vTaskDelay(pdMS_TO_TICKS(1000));  // Wait before retry
                    }
                    success = sendPost(request.endpoint, request.payload);
                }

                if (!success) {
                    ESP_LOGE("HTTP", "Request failed after %d attempts",
                             MAX_RETRIES);
                    // Optional: store failed requests for later retry
                }
            }
        }
    }

    // ─────────────────────────────────────────────
    // Public API
    // ─────────────────────────────────────────────

    void init() {
        // Cache MAC address
        uint8_t mac[6];
        WiFi.macAddress(mac);
        char macStr[18];
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        macAddress = String(macStr);

        // Create request queue
        requestQueue = xQueueCreate(QUEUE_SIZE, sizeof(HttpRequest));

        if (requestQueue == nullptr) {
            ESP_LOGE("HTTP", "Failed to create request queue");
            return;
        }

        // Create HTTP task
        xTaskCreatePinnedToCore(httpTask, "HttpTask",
                                8192,  // Needs stack for HTTP/TLS
                                nullptr,
                                1,  // Low priority
                                &httpTaskHandle,
                                0  // Core 0 (keep networking off Core 1)
        );

        ESP_LOGI("HTTP", "HTTP service initialized, MAC: %s",
                 macAddress.c_str());
    }

    void queuePost(const String& endpoint, const String& jsonPayload) {
        if (requestQueue == nullptr) {
            ESP_LOGE("HTTP", "Queue not initialized");
            return;
        }

        HttpRequest request;
        request.endpoint = endpoint;
        request.payload = jsonPayload;

        // Non-blocking queue send (drop if full)
        if (xQueueSend(requestQueue, &request, 0) != pdTRUE) {
            ESP_LOGW("HTTP", "Request queue full, dropping request");
        }
    }

    String getMacAddress() { return macAddress; }

}
