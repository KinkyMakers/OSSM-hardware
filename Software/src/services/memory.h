#ifndef MEMORY_H
#define MEMORY_H

#include <Arduino.h>
#include <ArduinoJson.h>

extern JsonDocument registry;

bool initLittleFS();

#endif  // MEMORY_H
