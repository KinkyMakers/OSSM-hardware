#pragma once

#include <ArduinoJson.h>
#include <RadBle.h>

extern radble::Server radBleServer;

bool initRadBle(NimBLEServer* server);

// Same handler the BLE request characteristic uses. Serial and other
// in-process callers skip the control lease.
radble::Result dispatchRadCommand(JsonObjectConst request);
