#include <esp_now.h>
#include "controllers/m5remote.hpp"

int hardwareVersion = 10; // V2.7 = integer value 27

volatile float speedPercentage = 0;
volatile float sensation = 0;

// Variable to store if sending data was successful
String success;

float out_esp_speed;
float out_esp_depth;
float out_esp_stroke;
float out_esp_sensation;
float out_esp_pattern;
bool out_esp_rstate;
bool out_esp_connected;
int out_esp_command;
float out_esp_value;
int out_esp_target;

float incoming_esp_speed;
float incoming_esp_depth;
float incoming_esp_stroke;
float incoming_esp_sensation;
float incoming_esp_pattern;
bool incoming_esp_rstate;
bool incoming_esp_connected;
bool incoming_esp_heartbeat;
int incoming_esp_command;
float incoming_esp_value;
int incoming_esp_target;

bool m5_first_connect = false;
bool heartbeat = false;
bool m5_remotelost = false;

struct_message outgoingcontrol;
struct_message incomingcontrol;

esp_now_peer_info_t peerInfo;
uint32_t Token = 1111;

// Callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
}

// Callback when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingcontrol, incomingData, sizeof(incomingcontrol));
  switch(incomingcontrol.esp_target)
  {
    case OSSM_ID:
    {
    if(m5_first_connect == true && m5_remotelost == false){
    LogDebug(incomingcontrol.esp_command);
    LogDebug(incomingcontrol.esp_value);
    switch(incomingcontrol.esp_command)
    {
      case ON:
      {
      LogDebug("ON Got");
      Stroker.startPattern();
      outgoingcontrol.esp_command = ON;
      esp_err_t result = esp_now_send(Broadcast_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
      }
      break;
      case OFF:
      {
      LogDebug("OFF Got");
      Stroker.stopMotion();
      outgoingcontrol.esp_command = OFF;
      esp_err_t result = esp_now_send(Broadcast_Address, (uint8_t *) &outgoingcontrol, sizeof(outgoingcontrol));
      }
      break;
      case SPEED:
      {
      speed = incomingcontrol.esp_value; 
      Stroker.setSpeed(speed, true);
      }
      break;
      case DEPTH:
      {
      depth = incomingcontrol.esp_value;
      Stroker.setDepth(depth, true);
      }
      break;
      case STROKE:
      {
      stroke = incomingcontrol.esp_value;
      Stroker.setStroke(stroke, true);
      }
      break;
      case SENSATION:
      {
      sensation = incomingcontrol.esp_value;
      Stroker.setSensation(sensation, true);
      }
      break;
      case PATTERN:
      {
      int patter = incomingcontrol.esp_value;
      Stroker.setPattern(patter, true);
      LogDebug(Stroker.getPatternName(patter));
      }
      break;
      case TORQE_F:
      {
        int torqe = incomingcontrol.esp_value * 10;
        LogDebug(torqe);
        Error err = MB.addRequest(Token++, 1, WRITE_HOLD_REGISTER, 0x01FE, torqe);
        if (err!=SUCCESS) {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
        }
      }
      break;
      case TORQE_R:
      {
        int torqe = 65535 - (incomingcontrol.esp_value * -10);
        LogDebug(torqe);
        Error err = MB.addRequest(Token++, 1, WRITE_HOLD_REGISTER, 0x01FF, torqe);
        if (err!=SUCCESS) {
        ModbusError e(err);
        Serial.printf("Error creating request: %02X - %s\n", (int)e, (const char *)e);
        }
      }
      break;
      case SETUP_D_I:
      Stroker.setupDepth(10, false);
      break;
      case SETUP_D_I_F:
      Stroker.setupDepth(10, true);
      break;
      case REBOOT:
      ESP.restart();
      break; 
      
    }
    } else if(m5_first_connect == false && m5_remotelost == false && incomingcontrol.esp_command == HEARTBEAT && incomingcontrol.esp_heartbeat == true){
        m5_first_connect = true;
        Serial.printf("Got M5 connection, restarting homeing\n");
        Stroker.disable();
        if (hardwareVersion >= 20)
        {
          Stroker.enableAndSensorlessHome(&sensorless, homingNotification, 10);
        }
        else
        {
          Stroker.enableAndHome(&endstop, homingNotification); // pointer to the homing config struct
        }
    }
  }
  }
  
}

void M5Remote::_attachTask() {
  xTaskCreatePinnedToCore(
    std::bind(&M5Remote::_tick, &this),      /* Task function. */
    "espNowRemoteTask",  /* name of task. */
    4096,               /* Stack size of task */
    NULL,               /* parameter of the task */
    5,                  /* priority of the task */
    &eRemote_t,         /* Task handle to keep track of created task */
    0                   /* pin task to core 0 */
  );        
}

void M5Remote::_tick(void *pvParameters)
{
    // Once ESPNow is successfully Init, we will register for Send CB to
    // get the status of Trasnmitted packet
    esp_now_register_send_cb(OnDataSent);

    // Register peer
    memcpy(peerInfo.peer_addr, Broadcast_Address, 6);
    peerInfo.channel = 0;  
    peerInfo.encrypt = false;
  
    // Add peer
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
      ESP_LOGE("m5remote", "Failed to add peer");
      vTaskSuspend();
    return;
    } else {
      vTaskSuspend(CRemote_T);
    }
    // Register for a callback function that will be called when data is received
    esp_now_register_recv_cb(OnDataRecv);

    for(;;)
    {
      vTaskDelay(500);
    }
}