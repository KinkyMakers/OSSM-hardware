#include <Arduino.h>
#include <utils/random.h>

#include "FastAccelStepper.h"
#include "communication/http.h"
#include "esp_log.h"

namespace Telemetry {

    struct PositionSample {
        unsigned long time;  // Timestamp in milliseconds
        float position;      // Position in millimeters
    };

    class PositionBuffer {
      private:
        static const int BUFFER_SIZE = 32;
        PositionSample buffer[BUFFER_SIZE];
        int writeIndex = 0;
        int sampleCount = 0;

      public:
        void addSample(unsigned long time, float position) {
            buffer[writeIndex].time = time;
            buffer[writeIndex].position = position;

            // Move write pointer forward, wrap around if at end
            writeIndex = (writeIndex + 1) % BUFFER_SIZE;

            // Track how many samples we have (max BUFFER_SIZE)
            if (sampleCount < BUFFER_SIZE) {
                sampleCount++;
            }
        }

        int getCount() const { return sampleCount; }
        /**
         * Clear all samples from the buffer
         */
        void clear() {
            sampleCount = 0;
            writeIndex = 0;
        }

        /**
         * Build JSON array string for all samples
         * Format: [{"time": 123, "position": 45.6}, ...]
         */
        String drainToJson() {
            String json = "[";
            for (int i = 0; i < sampleCount; i++) {
                if (i > 0) json += ",";
                int idx =
                    (writeIndex - sampleCount + i + BUFFER_SIZE) % BUFFER_SIZE;
                PositionSample sample = buffer[idx];
                json += "{\"time\":";
                json += String(sample.time);
                json += ",\"position\":";
                json += String(sample.position, 2);  // 2 decimal places
                json += "}";
            }
            json += "]";

            this->clear();  // Clear buffer after exporting

            return json;
        }
    };

    // ─────────────────────────────────────────────
    // State Variables
    // ─────────────────────────────────────────────
    static PositionBuffer positionBuffer;
    static SemaphoreHandle_t bufferMutex = nullptr;
    static FastAccelStepper* stepperRef = nullptr;

    static volatile bool sessionActive = false;
    static String sessionId = "";

    static TaskHandle_t samplingTaskHandle = nullptr;
    static TaskHandle_t uploadTaskHandle = nullptr;

    // ─────────────────────────────────────────────
    // Helper Functions
    // ─────────────────────────────────────────────

    static String generateSessionId() { return uuid(); }

    static String buildJsonPayload(String sampleJson) {
        String json = "{";
        json += "\"macAddress\":\"";
        json += HttpService::getMacAddress();
        json += "\"";
        json += ",\"sessionId\":\"";
        json += sessionId;
        json += "\"";
        json += ",\"data\":";
        json += sampleJson;
        json += "}";
        return json;
    }

    // ─────────────────────────────────────────────
    // Background Tasks
    // ─────────────────────────────────────────────
    static void positionSamplingTask(void* pvParameters) {
        const TickType_t sampleInterval =
            pdMS_TO_TICKS(33);  // ~30Hz (1000ms / 30 = 33ms)

        while (true) {
            // If telemetry session is not active, sleep briefly and continue
            if (!sessionActive) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            TickType_t lastSampleTime = xTaskGetTickCount();

            unsigned long timestamp = millis();
            float position = stepperRef->getCurrentPosition();

            // Thread-safe write to buffer
            if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                positionBuffer.addSample(timestamp, position);
                xSemaphoreGive(bufferMutex);

                ESP_LOGI("Telemetry", "Sampled position: %.2f mm at %lu ms",
                         position, timestamp);
            }

            vTaskDelayUntil(&lastSampleTime, sampleInterval);
        }
    }

    static void telemetryUploadTask(void* pvParameters) {
        const TickType_t uploadInterval = pdMS_TO_TICKS(1000);  // 1Hz

        while (true) {
            // Sleep while session is inactive
            if (!sessionActive) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            vTaskDelay(uploadInterval);

            // Drain buffer (thread-safe)
            String samplesJson;
            if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
                if (positionBuffer.getCount() > 0) {
                    samplesJson = positionBuffer.drainToJson();
                }
                xSemaphoreGive(bufferMutex);
            }

            // Send if we have data
            if (samplesJson.length() > 2) {  // More than "[]"
                String payload = buildJsonPayload(samplesJson);
                // Hand off to HTTP service (non-blocking)
                HttpService::queuePost("/api/ossm", payload);
            }
        }
    }
    // ─────────────────────────────────────────────
    // Public API
    // ─────────────────────────────────────────────
    void init(FastAccelStepper* stepper) {
        stepperRef = stepper;
        bufferMutex = xSemaphoreCreateMutex();

        // Create tasks once at startup (they sleep when inactive)
        xTaskCreatePinnedToCore(positionSamplingTask, "TelemetrySample", 2048,
                                nullptr,
                                1,  // Low priority
                                &samplingTaskHandle,
                                0  // Core 0
        );

        xTaskCreatePinnedToCore(telemetryUploadTask, "TelemetryUpload",
                                4096,  // Needs more stack for HTTP/JSON
                                nullptr, 1, &uploadTaskHandle, 0);

        ESP_LOGI("Telemetry", "Telemetry service initialized");
    }

    void startSession(FastAccelStepper* stepper) {
        if (sessionActive) return;  // Already active

        sessionId = generateSessionId();

        // Clear any stale data
        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            positionBuffer.clear();
            xSemaphoreGive(bufferMutex);
        }

        sessionActive = true;
        ESP_LOGI("Telemetry", "Session started: %s", sessionId.c_str());
    }

    void endSession() {
        if (!sessionActive) return;

        sessionActive = false;

        ESP_LOGI("Telemetry", "Session ended: %s", sessionId.c_str());
        sessionId = "";
    }
}
