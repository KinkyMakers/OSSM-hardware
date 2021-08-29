// Adafruit IO Device Information
// desc: Displays Device, WiFi, and Adafruit IO connection information
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Brent Rubell for Adafruit Industries
// Copyright (c) 2018 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/
// device mac address
byte mac[6];

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  Serial.print("Connecting to Adafruit IO...");

  // connect to io.adafruit.com
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  // Device Info
  Serial.println("----DEVICE INFO----");
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  WiFi.macAddress(mac);
  Serial.print("MAC Address: ");
  for(int i=0;i<6;i++) {
    Serial.print(mac[i], HEX);
  }
  Serial.println();

  // Network Info
  Serial.println("----ROUTER INFO----");
  Serial.print("WIFI SSID: ");
  Serial.println(WIFI_SSID);
  Serial.print("WIFI Pass: ");
  Serial.println(WIFI_PASS);
  long rssi = WiFi.RSSI();
  Serial.print("RSSI:");
  Serial.println(rssi);

  // Adafruit IO Info
  Serial.println("----ADAFRUIT IO INFO----");
  Serial.print("IO User: ");
  Serial.println(IO_USERNAME);
  Serial.print("IO Key: ");
  Serial.println(IO_KEY);
  Serial.print("IO Status: ");  
  Serial.println(io.statusText());

}

void loop(){
  
}

