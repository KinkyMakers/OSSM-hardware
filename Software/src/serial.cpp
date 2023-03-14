// Based on https://www.hackster.io/techbase_group/arduino-esp32-serial-port-to-tcp-converter-via-wifi-66d341
#include "config.hpp"

#include "WiFi.h"
#include "AsyncTCP.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "serial.hpp"

#define BUFFER_SIZE VCP_BUFFER_SIZE
#define SERIAL_RX_PIN 44
#define SERIAL_TX_PIN 43
const int baudrate = VCP_BAUD_RATE;
const int rs_config = SERIAL_8N1;

//WiFiServer serial_server;
AsyncServer* serial_server = NULL;
AsyncClient* serial_activeClient = NULL;
char* buff[BUFFER_SIZE];
char hexBuff[2048]; // TODO - Encapsulate in anonymous namespace

// https://stackoverflow.com/a/44749986
void array_to_string(byte array[], unsigned int len, char buffer[])
{
    for (unsigned int i = 0; i < len; i++)
    {
        byte nib1 = (array[i] >> 4) & 0x0F;
        byte nib2 = (array[i] >> 0) & 0x0F;
        buffer[i*3+0] = nib1  < 0xA ? '0' + nib1  : 'A' + nib1  - 0xA;
        buffer[i*3+1] = nib2  < 0xA ? '0' + nib2  : 'A' + nib2  - 0xA;
        buffer[i*3+2] = ' ';
    }
    buffer[len*3] = '\0';
}

static void handleData(void *arg, AsyncClient *client, void *data, size_t len)
{
  array_to_string((byte*)data, len, hexBuff);
	ESP_LOGI("serial", "data received from client %s \n", client->remoteIP().toString().c_str());
  ESP_LOGI("serial", "Client Data %u: %s", len, hexBuff);
  
  Serial2.write((char*)data, len);
  Serial2.flush();
}

static void handleError(void *arg, AsyncClient *client, int8_t error)
{
  serial_activeClient = NULL;
	ESP_LOGI("serial", "connection error %s from client %s \n", client->errorToString(error), client->remoteIP().toString().c_str());
}

static void handleDisconnect(void *arg, AsyncClient *client)
{
  serial_activeClient = NULL;
	ESP_LOGI("serial", "client %s disconnected \n", client->remoteIP().toString().c_str());
}

static void handleTimeOut(void *arg, AsyncClient *client, uint32_t time)
{
  serial_activeClient = NULL;
	ESP_LOGI("serial", "client ACK timeout ip: %s \n", client->remoteIP().toString().c_str());
}

static void serial_handleNewClient(void* arg, AsyncClient* client) {
	ESP_LOGI("serial", "new client has been connected to server, ip: %s", client->remoteIP().toString().c_str());

  serial_activeClient = client;
	client->onData(&handleData, NULL);
	client->onError(&handleError, NULL);
	client->onDisconnect(&handleDisconnect, NULL);
	client->onTimeout(&handleTimeOut, NULL);
}

void serial_setup() {
  ESP_LOGI("serial", "Starting RS232 TCP Server");
  //serial_server = WiFiServer(3000);
  //serial_server.begin();

  Serial2.begin(baudrate, rs_config, SERIAL_RX_PIN, SERIAL_TX_PIN);

  serial_server = new AsyncServer(3000);
  serial_server->onClient(&serial_handleNewClient, serial_server);
  serial_server->begin();

  xTaskCreatePinnedToCore(&serial_task, "serial_task", 4096, NULL, 5, NULL, 1);
}

void serial_task(void* pvParameter) {
  //WiFiClient client = NULL; // TODO - Fix this warning
  size_t size = 0;

  while (true) {
    if (serial_activeClient != NULL && serial_activeClient->canSend()) {
      while ((size = Serial2.available())) {
        size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
        Serial2.readBytes((char*)buff, size);
        serial_activeClient->add((const char*)buff, size);
        serial_activeClient->send();

        array_to_string((byte*)buff, size, hexBuff);
        ESP_LOGI("serial", "Server Data %u: %s", size, hexBuff);

        vTaskDelay(1 / portTICK_PERIOD_MS);
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);
    /*
    while (!(client = serial_server.available())) {
      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGI("RS232", "RS232: Client connected");
    while (client.connected()) {
        
      // read data from wifi client and send to serial
      while ((size = client.available())) {
                size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
                client.read(buff, size);
                Serial2.write(buff, size);
                Serial2.flush();

                ESP_LOGI("RS232", "RS232: Client Data Start");
                Serial.write(buff, size); // Send to debug port
                ESP_LOGI("RS232", "RS232: Client Data End");
      }

      // read data from serial and send to wifi client
      while ((size = Serial2.available())) {
                size = (size >= BUFFER_SIZE ? BUFFER_SIZE : size);
                Serial2.readBytes(buff, size);
                client.write(buff, size);
                client.flush();

                ESP_LOGI("RS232", "RS232: Server Data Start");
                Serial.write(buff, size); // Send to debug port
                ESP_LOGI("RS232", "RS232: Server Data End");
      }

      vTaskDelay(10 / portTICK_PERIOD_MS);
    }

    ESP_LOGI("RS232", "RS232: Client disconnected");
    client.stop();
    client = NULL;
    */
  }
}
