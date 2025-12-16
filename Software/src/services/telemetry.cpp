#include <Arduino.h>
#include <utils/random.h>

#include <atomic>

#include "communication/http.h"
#include "esp_log.h"
#include "ossm/OSSM.h"

namespace Telemetry {

    struct Sample {
        unsigned long time;  // Timestamp in milliseconds
        float position;      // Position in millimeters
        float watts;     // Power in watts
    };

    class SampleBuffer {
      private:
        // Buffer sized for ~1 second of samples at 30Hz (30 samples) plus
        // safety margin. At 30Hz sampling and 1Hz upload, expect ~30 samples
        // per upload; 32 provides a small safety margin.
        static const int BUFFER_SIZE = 32;
        Sample buffer[BUFFER_SIZE];
        int writeIndex = 0;
        int sampleCount = 0;

      public:
        void addSample(unsigned long time, float position, float watts) {
            buffer[writeIndex].time = time;
            buffer[writeIndex].position = position;
            buffer[writeIndex].watts = watts;

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
         * Format: [{"time":123,"position":45.6,"watts":78.9},...]
         */
        String drainToJson() {
            // Pre-allocate: ~45 bytes per sample + overhead
            const int estimatedSize = sampleCount * 50 + 10;
            String json;
            json.reserve(estimatedSize);

            json = "[";

            char sampleBuffer[64];  // Fixed buffer for one sample
            for (int i = 0; i < sampleCount; i++) {
                int idx =
                    (writeIndex - sampleCount + i + BUFFER_SIZE) % BUFFER_SIZE;
                Sample& sample = buffer[idx];

                // Format directly into fixed buffer (no heap allocation)
                snprintf(sampleBuffer, sizeof(sampleBuffer),
                         "%s{\"time\":%lu,\"position\":%.2f,\"watts\":%.2f}", i > 0 ? "," : "",
                         sample.time, sample.position, sample.watts);

                json += sampleBuffer;
            }
            json += "]";

            this->clear();

            return json;
        }
    };

    // ─────────────────────────────────────────────
    // State Variables
    // ─────────────────────────────────────────────
    static SampleBuffer sampleBuffer;
    static SemaphoreHandle_t bufferMutex = nullptr;
    static OSSM* ossmRef = nullptr;

    static std::atomic<bool> sessionActive{false};
    static String sessionId = "";

    static TaskHandle_t samplingTaskHandle = nullptr;
    static TaskHandle_t uploadTaskHandle = nullptr;

    // ─────────────────────────────────────────────
    // Helper Functions
    // ─────────────────────────────────────────────

    static String generateSessionId() { return uuid(); }

    static String buildJsonPayload(String sampleJson) {
        String localSessionId;
        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            localSessionId = sessionId;  // Copy while protected
            xSemaphoreGive(bufferMutex);
        }
        String macAddress = HttpService::getMacAddress();

        // Pre-allocate: fixed parts (~60 bytes) + sessionId (~36 bytes) +
        // sampleJson
        String json;
        json.reserve(100 + localSessionId.length() + sampleJson.length());

        json = "{\"macAddress\":\"";
        json += macAddress;
        json += "\",\"sessionId\":\"";
        json += localSessionId;
        json += "\",\"data\":";
        json += sampleJson;
        json += "}";

        return json;
    }

    // ─────────────────────────────────────────────
    // Background Tasks
    // ─────────────────────────────────────────────
    static void samplingTask(void* pvParameters) {
        const TickType_t sampleInterval =
            pdMS_TO_TICKS(33);  // ~30Hz (1000ms / 30 = 33ms)

        while (true) {
            // If telemetry session is not active, sleep briefly and continue
            if (!sessionActive || ossmRef == nullptr) {
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            TickType_t lastSampleTime = xTaskGetTickCount();

            unsigned long timestamp = millis();
            float position = ossmRef->stepper->getCurrentPosition();

            // Ballpark approximate power consumption
            // Will vary from machine to machine
            float watts = 4*(getAnalogAveragePercent(
                        SampleOnPin{Pins::Driver::currentSensorPin, 3}) -
                    ossmRef->currentSensorOffset);


            // Thread-safe write to buffer
            if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                sampleBuffer.addSample(timestamp, position, watts);
                xSemaphoreGive(bufferMutex);
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
                if (sampleBuffer.getCount() > 0) {
                    samplesJson = sampleBuffer.drainToJson();
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
    void init() {
        bufferMutex = xSemaphoreCreateMutex();

        // Create tasks once at startup (they sleep when inactive)
        xTaskCreatePinnedToCore(samplingTask, "TelemetrySample", 3072,
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

    void startSession(OSSM* ossm) {
        if (sessionActive) return;  // Already active

        ossmRef = ossm;

        // Clear any stale data
        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            sessionId = generateSessionId();
            sampleBuffer.clear();
            xSemaphoreGive(bufferMutex);
        }

        // COmmented out to see if it has any effect on telemetry timing
        // Small delay before activating to let motor ramp up
        // vTaskDelay(pdMS_TO_TICKS(100));

        sessionActive = true;
        ESP_LOGI("Telemetry", "Session started: %s", sessionId.c_str());
    }

    void endSession() {
        if (!sessionActive) return;

        sessionActive = false;

        vTaskDelay(pdMS_TO_TICKS(100));  // give some time for last samples to
                                         // be sent before clearning session ID
        // Protect sessionId write
        String endedSessionId;
        if (xSemaphoreTake(bufferMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            endedSessionId = sessionId;  // Copy for logging
            sessionId = "";
            xSemaphoreGive(bufferMutex);
        }

        ESP_LOGI("Telemetry", "Session ended: %s", endedSessionId.c_str());
    }
}
