#include "websocket.hpp"

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      // TODO - Might support incoming data at some point, but not a concern right now
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void onConnect(AsyncEventSourceClient *client) {
    if(client->lastId()){
      Serial.printf("EventSource Client reconnected! Last message ID that it gat is: %u\n", client->lastId());
    } else {
      Serial.printf("EventSource Client connected!");
    }
    //send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!",NULL,millis(),1000);
}