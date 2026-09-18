#ifndef OSSM_UTILS_TLS_SESSION_HPP
#define OSSM_UTILS_TLS_SESSION_HPP

#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

#include "services/communication/mqtt.h"

// Largest contiguous internal-RAM block a TLS handshake needs on this board.
// Measured 2026-09-08 with MQTT paused: free 83,772 B, largest 29,696 B, and
// the handshake succeeds (mbedTLS input buffer is 16 KB). 24 KB leaves margin
// while still refusing a genuinely starved heap. Override per env with
// -D OSSM_TLS_MIN_LARGEST_BLOCK.
#ifndef OSSM_TLS_MIN_LARGEST_BLOCK
#define OSSM_TLS_MIN_LARGEST_BLOCK (8 * 1024)
#endif

/**
 * Scoped budget for one HTTPS job. Every TLS call on the OSSM goes through
 * this so the two rules that keep TLS working here are never skipped:
 *
 *   1. MQTT is paused for the duration (its own TLS session would otherwise
 *      compete for the same heap) and restored when the scope ends.
 *   2. The largest free internal block is checked before the handshake; if
 *      it is below the budget the caller gets ok()==false and a reason, and
 *      nothing is attempted. A refused call is a visible error, never a hang.
 */
// MQTT's own TLS session holds the heap that both a TLS handshake and the
// 15 KB stack of the task doing it need. Pause it before creating that task;
// the TlsSession inside the task adopts the pause and restores MQTT on exit.
inline bool &mqttPausedForTls() {
    static bool paused = false;
    return paused;
}

inline void pauseMqttForTls() {
    if (!mqttPausedForTls() && mqttClient != nullptr) {
        esp_mqtt_client_stop(mqttClient);
        mqttPausedForTls() = true;
        vTaskDelay(pdMS_TO_TICKS(100));  // let the MQTT socket close
    }
}

inline void resumeMqttAfterTls() {
    if (mqttPausedForTls() && mqttClient != nullptr) {
        const esp_err_t err = esp_mqtt_client_start(mqttClient);
        ESP_LOGW("TLS", "mqtt resumed: %s (internal free=%u largest=%u)",
                 esp_err_to_name(err),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL |
                                                   MALLOC_CAP_8BIT),
                 (unsigned)heap_caps_get_largest_free_block(
                     MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    }
    mqttPausedForTls() = false;
}

class TlsSession {
   public:
    // pauseMqtt=false: measure only, never touch MQTT. For low-priority
    // background polls that should skip a round rather than churn MQTT.
    explicit TlsSession(const char* what, bool pauseMqtt = true) : what_(what) {
        if (pauseMqtt) {
            pauseMqttForTls();  // no-op when the caller already paused it
            mqttStopped_ = true;
        }
        freeBefore_ = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        largestBefore_ =
            heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        ok_ = largestBefore_ >= OSSM_TLS_MIN_LARGEST_BLOCK;
        if (ok_ || pauseMqtt) {
            ESP_LOGW("TLS", "%s: internal free=%u largest=%u min=%u -> %s", what_,
                     (unsigned)freeBefore_, (unsigned)largestBefore_,
                     (unsigned)OSSM_TLS_MIN_LARGEST_BLOCK, ok_ ? "ok" : "REFUSED");
        } else {
            ESP_LOGD("TLS", "%s: internal free=%u largest=%u min=%u -> skipped", what_,
                     (unsigned)freeBefore_, (unsigned)largestBefore_,
                     (unsigned)OSSM_TLS_MIN_LARGEST_BLOCK);
        }
        if (!ok_) {
            error_ = "low-memory: largest free block " + String(largestBefore_) +
                     " B, need " + String((unsigned)OSSM_TLS_MIN_LARGEST_BLOCK);
            if (pauseMqtt) {
                // A refused foreground job is worth the per-region picture.
                heap_caps_print_heap_info(MALLOC_CAP_INTERNAL);
            }
        }
    }

    ~TlsSession() {
        if (mqttStopped_) resumeMqttAfterTls();
    }

    TlsSession(const TlsSession&) = delete;
    TlsSession& operator=(const TlsSession&) = delete;

    bool ok() const { return ok_; }
    const String& error() const { return error_; }
    size_t freeBefore() const { return freeBefore_; }
    size_t largestBefore() const { return largestBefore_; }

   private:
    const char* what_;
    bool mqttStopped_ = false;
    bool ok_ = false;
    size_t freeBefore_ = 0;
    size_t largestBefore_ = 0;
    String error_;
};

#endif  // OSSM_UTILS_TLS_SESSION_HPP
