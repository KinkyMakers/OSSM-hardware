// Adafruit IO Environmental Data Logger 
// Tutorial Link: https://learn.adafruit.com/adafruit-io-air-quality-monitor
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

/************************** Adafruit IO Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/**************************** Sensor Configuration ***************************************/
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "Adafruit_VEML6070.h"
#include "Adafruit_SGP30.h"

// BME280 Sensor Definitions
#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10
#define SEALEVELPRESSURE_HPA (1013.25)

// Instanciate the sensors
Adafruit_BME280 bme;
Adafruit_VEML6070 uv = Adafruit_VEML6070();
Adafruit_SGP30 sgp;

/**************************** Example ***************************************/
// Delay between sensor reads, in seconds
#define READ_DELAY 10

// DHT22 Data
int temperatureReading;
int pressureReading;

// SGP30 Data
int tvocReading = 0;
int ecO2Reading = 0;

// BME280 Data
int altitudeReading = 0;
int humidityReading = 0;

// VEML6070 Data
int uvReading = 0;

// set up the feeds for the BME280
AdafruitIO_Feed *temperatureFeed = io.feed("temperature");
AdafruitIO_Feed *humidityFeed = io.feed("humidity");
AdafruitIO_Feed *pressureFeed = io.feed("pressure");
AdafruitIO_Feed *altitudeFeed = io.feed("altitude");

// set up feed for the VEML6070
AdafruitIO_Feed *uvFeed = io.feed("uv");

// set up feeds for the SGP30
AdafruitIO_Feed *tvocFeed = io.feed("tvoc");
AdafruitIO_Feed *ecO2Feed = io.feed("ecO2");

void setup() {
  // start the serial connection
  Serial.begin(9600);

  // wait for serial monitor to open
  while (!Serial);

  Serial.println("Adafruit IO Environmental Logger");

  // set up BME280
  setupBME280();
  // set up SGP30
  setupSGP30();
  // setup VEML6070
  uv.begin(VEML6070_1_T);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while (io.status() < AIO_CONNECTED)
  {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());
}

void loop() {
  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

  Serial.println("Reading Sensors...");

  // Read the temperature from the BME280
  temperatureReading = bme.readTemperature();

  // convert from celsius to degrees fahrenheit
  temperatureReading = temperatureReading * 1.8 + 32;
  
  Serial.print("Temperature = "); Serial.print(temperatureReading); Serial.println(" *F");

  // Read the pressure from the BME280
  pressureReading = bme.readPressure() / 100.0F;
  Serial.print("Pressure = "); Serial.print(pressureReading); Serial.println(" hPa");

  // Read the altitude from the BME280
  altitudeReading = bme.readAltitude(SEALEVELPRESSURE_HPA);
  Serial.print("Approx. Altitude = "); Serial.print(altitudeReading); Serial.println(" m");
  
  // Read the humidity from the BME280
  humidityReading = bme.readHumidity();
  Serial.print("Humidity = "); Serial.print(humidityReading); Serial.println("%");

  // VEML6070
  uvReading = uv.readUV();
  Serial.print("UV Light Level: "); Serial.println(uvReading);

  if(! sgp.IAQmeasure()){
  tvocReading = -1;
  ecO2Reading = -1;  
  }
  else
  {
  tvocReading = sgp.TVOC;
  ecO2Reading = sgp.eCO2;  
  }
  
  Serial.print("TVOC: "); Serial.print(tvocReading); Serial.print(" ppb\t");
  Serial.print("eCO2: "); Serial.print(ecO2Reading); Serial.println(" ppm");

  // send data to Adafruit IO feeds
  temperatureFeed->save(temperatureReading);
  humidityFeed->save(humidityReading);
  altitudeFeed->save(altitudeReading);
  pressureFeed->save(pressureReading);
  uvFeed->save(uvReading);
  ecO2Feed->save(ecO2Reading);
  tvocFeed->save(tvocReading);

  // delay the polled loop
  delay(READ_DELAY * 1000);
}

// Set up the SGP30 sensor
void setupSGP30() {
  if (!sgp.begin())
  {
    Serial.println("Sensor not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // If you previously calibrated the sensor in this environment,
  // you can assign it to self-calibrate (replace the values with your baselines):
  // sgp.setIAQBaseline(0x8E68, 0x8F41);
}

// Set up the BME280 sensor
void setupBME280() {
  bool status;
  status = bme.begin();
  if (!status)
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  Serial.println("BME Sensor is set up!");
}
