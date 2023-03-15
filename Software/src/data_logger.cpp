#include "data_logger.hpp"
#include <ArduinoJson.h>

const char* WebLogLevel_KEYS[WebLogLevel_NUM_OF_ITEMS] = {
  "verbose", "debug", "info", "warning", "error"
};

const char* DataParameter_NAMES[DataParameter_NUM_OF_ITEMS] = {
  "Actual Position",
  "Actual Velocity",
  "Actual Force",

  "Demand Position",
  "Demand Velocity",

  "Motor Supply Voltage",
  "Motor Power Losses",
  "Model Temp",
  "Real Temp"
};
const char* DataParameter_KEYS[DataParameter_NUM_OF_ITEMS] = {
  "apos",
  "avel",
  "aforce",

  "dpos",
  "dvel",

  "vsp",
  "wloss",
  "mtemp",
  "rtemp"
};

WebLogger wlog;

void WebLogger::startTask() {
  xTaskCreatePinnedToCore(&WebLogger::sendData, "weblogger_send_data", 10000, this, 5, &this->sendDataHandle, 1);
  xTaskCreatePinnedToCore(&WebLogger::sendHeartbeat, "weblogger_send_heartbeat", 2000, this, 5, &this->sendHeartbeatHandle, 1);
}

void WebLogger::log(DataParameter key, float value) {
  if (xSemaphoreTake(this->datapoints_writeLock, portMAX_DELAY) != pdTRUE) {
    // TODO - Log error
    return;
  }

  int64_t now = esp_timer_get_time();

  uint8_t keyIndex = static_cast<uint8_t>(key);
  uint8_t writeHead = datapoints_writeHead[keyIndex];
  datapoints[keyIndex][writeHead] = {now, value};
  datapoints_writeHead[keyIndex] = (datapoints_writeHead[keyIndex] + 1) % WEBLOGGER_BUFFER_SIZE;

  xSemaphoreGive(datapoints_writeLock);
}

void WebLogger::log(WebLogLevel level, char* format, ...) {
  

  if (this->ws == NULL) {
    return;
  }

  // TODO - Combine with standard log printf to serial
  static char loc_buf[64];
  char * temp = loc_buf;
  int len;
  va_list arg;
  va_list copy;
  va_start(arg, format);
  va_copy(copy, arg);
  len = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if(len >= sizeof(loc_buf)){
      temp = (char*)malloc(len+1);
      if(temp == NULL) {
          va_end(arg);
          return;
      }
  }

  vsnprintf(temp, len+1, format, arg);

  StaticJsonDocument<1024> doc;
  doc["type"] = "log";
  doc["level"] = WebLogLevel_KEYS[static_cast<uint8_t>(level)];
  doc["msg"] = temp;
  
  size_t jsLen = measureJson(doc);
  char* jsBuffer = (char*)malloc(jsLen+1);
  serializeJson(doc, jsBuffer, jsLen + 1);

  ws->textAll(jsBuffer);

  free(jsBuffer);
  
  va_end(arg);
  if(len >= sizeof(loc_buf)){
      free(temp);
  }
}

void WebLogger::sendData() {
  if (this->es == NULL) {
    return;
  }

  if (xSemaphoreTake(this->datapoints_readLock, portMAX_DELAY) != pdTRUE) {
    // TODO - Log error
    return;
  }

  // TODO - Switch to a more compact encoding style
  StaticJsonDocument<4064> doc;

  uint8_t updatesToSend = 0;
  for (uint8_t i = 0; i < DataParameter_NUM_OF_ITEMS; i++) {
    uint8_t writeHead = datapoints_writeHead[i];
    uint8_t readHead = datapoints_readHead[i];
    uint8_t itemsToSend;

    if (writeHead > readHead) {
      // r x x x x w = 5 item (w = 5, r = 0, t = 6, w - r)
      // x r x x w x = 3 items (w = 4, r = 1, t = 6, w - r)
      itemsToSend = writeHead - readHead;
    } else if (readHead > writeHead) {
      // w x x x x r = 1 item (w = 0, r = 5, t = 6, (t - r) + w)
      // x w x x r x = 3 items (w = 1, r = 4, t = 6, (t - r) + w)
      itemsToSend = (WEBLOGGER_BUFFER_SIZE - readHead) + writeHead;
    } else { // readHead == writeHead
      continue;
    }

    const char* key = DataParameter_KEYS[i];
    JsonArray dataPoints = doc.createNestedArray(key);

    uint8_t head = readHead;
    for (uint8_t x = 0; x < itemsToSend; x++) {
      JsonArray point = dataPoints.createNestedArray();
      point.add(datapoints[i][head].time / 1000000.0);
      point.add(datapoints[i][head].value);
      head = (head + 1) % WEBLOGGER_BUFFER_SIZE;
    }

    datapoints_readHead[i] = writeHead;
    updatesToSend += 1;
  }

  if (updatesToSend == 0) {
    xSemaphoreGive(datapoints_readLock);
    return;
  }

  size_t jsLen = measureJson(doc);
  char* jsBuffer = (char*)malloc(jsLen+1);
  serializeJson(doc, jsBuffer, jsLen + 1);

  es->send(jsBuffer, "points", millis());
  free(jsBuffer);
  
  xSemaphoreGive(datapoints_readLock);
}

void WebLogger::sendHeartbeat() {
  if (this->es == NULL) {
    return;
  }

  es->send("", "heartbeat", millis());
}