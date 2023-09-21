#pragma once

#include "ESPAsyncWebServer.h"
#include <ESPDash.h>
#include "SPIFFS.h"

extern AsyncWebServer server;
extern AsyncWebSocket ws;
extern AsyncEventSource events;

void server_setup();