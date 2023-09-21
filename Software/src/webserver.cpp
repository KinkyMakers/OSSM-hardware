#include "webserver.hpp"
#include "websocket.hpp"

AsyncWebServer server(80);
AsyncWebSocket ws("/");
AsyncEventSource events("/es");

void server_setup() {
  log_i("Starting Web Server");
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
  DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));

  server.serveStatic("/www2", SPIFFS, "/www/").setDefaultFile("index.html");
  server.onNotFound([](AsyncWebServerRequest *request) {
      Serial.printf("Not found: %s!\r\n", request->url().c_str());
      request->send(404);
   });

  server.begin();

  ws.onEvent(onEvent);
  events.onConnect((ArEventHandlerFunction)onConnect);
  server.addHandler(&ws);
  server.addHandler(&events);

#if WEBLOG_AVAILABLE == 1
  wlog.attachWebsocket(&ws);
  wlog.attachEventsource(&events);
  wlog.startTask();
#endif
}