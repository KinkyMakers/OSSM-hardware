// Native test shim for ESP-IDF logging. Real device uses esp_log.h from
// ESP-IDF; under the native test platform we just stringify to the compiler's
// output to avoid a dependency on ESP-IDF.
#ifndef TEST_SPLINE_PATTERNS_ESP_LOG_H
#define TEST_SPLINE_PATTERNS_ESP_LOG_H

#include <stdio.h>

#define ESP_LOGE(tag, fmt, ...) \
    fprintf(stderr, "[E][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) \
    fprintf(stderr, "[W][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) \
    fprintf(stdout, "[I][%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGD(tag, fmt, ...) ((void)0)
#define ESP_LOGV(tag, fmt, ...) ((void)0)

typedef int esp_log_level_t;
static inline void esp_log_level_set(const char* tag, esp_log_level_t level) {
    (void)tag;
    (void)level;
}

#endif  // TEST_SPLINE_PATTERNS_ESP_LOG_H
