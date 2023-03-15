#pragma once

#include "config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "ESPAsyncWebServer.h"

enum class WebLogLevel {
  VERBOSE,
  DEBUG,
  INFO,
  WARNING,
  ERROR,

  NUM_OF_ITEMS
};
const uint8_t WebLogLevel_NUM_OF_ITEMS = static_cast<uint8_t>(WebLogLevel::NUM_OF_ITEMS);
extern const char* WebLogLevel_KEYS[];

enum class DataParameter {
  ACTUAL_POSITON,
  ACTUAL_VELOCITY,
  ACTUAL_FORCE,

  DEMAND_POSITON,
  DEMAND_VELOCITY,

  MOTOR_SUPPLY_VOLTAGE,
  MOTOR_POWER_LOSSES,
  MODEL_TEMP,
  REAL_TEMP,

  NUM_OF_ITEMS
};
const uint8_t DataParameter_NUM_OF_ITEMS = static_cast<uint8_t>(DataParameter::NUM_OF_ITEMS);
extern const char* DataParameter_NAMES[];
extern const char* DataParameter_KEYS[];

#define WEBLOGGER_BUFFER_SIZE 64

#define WEBLOG_LOG_FORMAT(format)  "[%s:%u] %s(): " format "\r\n", pathToFileName(__FILE__), __LINE__, __FUNCTION__

#if WEBLOG_AVAILABLE == 1
  #define WEB_LOG(level, format, ...) wlog.log(level, format, ##__VA_ARGS__)
#else
  #define WEB_LOG(level, format, ...) ;
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_VERBOSE
  #define weblog_v(format, ...) \
    log_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__); \
    WEB_LOG(WebLogLevel::VERBOSE, WEBLOG_LOG_FORMAT(format), ##__VA_ARGS__)
#else
  #define weblog_v(format, ...) do {} while(0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_DEBUG
  #define weblog_d(format, ...) \
    log_printf(ARDUHAL_LOG_FORMAT(V, format), ##__VA_ARGS__); \
    WEB_LOG(WebLogLevel::DEBUG, WEBLOG_LOG_FORMAT(format), ##__VA_ARGS__)
#else
  #define weblog_d(format, ...) do {} while(0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_INFO
  #define weblog_i(format, ...) \
    log_printf(ARDUHAL_LOG_FORMAT(I, format), ##__VA_ARGS__); \
    WEB_LOG(WebLogLevel::INFO, WEBLOG_LOG_FORMAT(format), ##__VA_ARGS__)
#else
  #define weblog_i(format, ...) do {} while(0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_WARN
  #define weblog_w(format, ...) \
    log_printf(ARDUHAL_LOG_FORMAT(W, format), ##__VA_ARGS__); \
    WEB_LOG(WebLogLevel::WARN, WEBLOG_LOG_FORMAT(format), ##__VA_ARGS__)
#else
  #define weblog_w(format, ...) do {} while(0)
#endif

#if ARDUHAL_LOG_LEVEL >= ARDUHAL_LOG_LEVEL_ERROR
  #define weblog_e(format, ...) \
    log_printf(ARDUHAL_LOG_FORMAT(E, format), ##__VA_ARGS__); \
    WEB_LOG(WebLogLevel::ERROR, WEBLOG_LOG_FORMAT(format), ##__VA_ARGS__)
#else
  #define weblog_e(format, ...) do {} while(0)
#endif

#define WEB_LOGE(tag, format, ...)  weblog_e("[%s] " format, tag, ##__VA_ARGS__)
#define WEB_LOGW(tag, format, ...)  weblog_w("[%s] " format, tag, ##__VA_ARGS__)
#define WEB_LOGI(tag, format, ...)  weblog_i("[%s] " format, tag, ##__VA_ARGS__)
#define WEB_LOGD(tag, format, ...)  weblog_d("[%s] " format, tag, ##__VA_ARGS__)
#define WEB_LOGV(tag, format, ...)  weblog_v("[%s] " format, tag, ##__VA_ARGS__)

struct datapoint {
  int64_t time;
  float value;
};

class WebLogger {
  public:
    void attachWebsocket(AsyncWebSocket *ws) {
      this->ws = ws;
    }

    void attachEventsource(AsyncEventSource *es) {
      this->es = es;
    }

    void startTask();
    void log(DataParameter key, float value);
    void log(WebLogLevel level, char* format, ...);

  private:
    AsyncWebSocket *ws;
    AsyncEventSource *es;

    static void sendData(void* _this) {
      while (true) {
        static_cast<WebLogger*>(_this)->sendData();
        
        // Attempt 60fps update (1/60 ~= 16.667ms)
        vTaskDelay(16.667 / portTICK_PERIOD_MS);
      }
    }
    void sendData();
    TaskHandle_t sendDataHandle = NULL;

    static void sendHeartbeat(void* _this) {
      while (true) {
        static_cast<WebLogger*>(_this)->sendHeartbeat();
        
        // Attempt 60fps update (1/60 ~= 16.667ms)
        vTaskDelay(16.667 / portTICK_PERIOD_MS);
      }
    }
    void sendHeartbeat();
    TaskHandle_t sendHeartbeatHandle = NULL;

    uint8_t datapoints_readHead[DataParameter_NUM_OF_ITEMS];
    uint8_t datapoints_writeHead[DataParameter_NUM_OF_ITEMS];
    datapoint datapoints[DataParameter_NUM_OF_ITEMS][WEBLOGGER_BUFFER_SIZE];

    SemaphoreHandle_t datapoints_readLock = xSemaphoreCreateMutex();
    SemaphoreHandle_t datapoints_writeLock = xSemaphoreCreateMutex();
};

extern WebLogger wlog;