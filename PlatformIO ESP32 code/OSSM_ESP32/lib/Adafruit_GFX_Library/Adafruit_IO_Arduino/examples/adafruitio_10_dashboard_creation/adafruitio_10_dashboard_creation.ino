// Adafruit IO Dashboard Setup Example
//
// Adafruit invests time and resources providing this open source code.
// Please support Adafruit and open source hardware by purchasing
// products from Adafruit!
//
// Written by Todd Treece for Adafruit Industries
// Copyright (c) 2016 Adafruit Industries
// Licensed under the MIT license.
//
// All text above must be included in any redistribution.

/************************** Configuration ***********************************/

// edit the config.h tab and enter your Adafruit IO credentials
// and any additional configuration needed for WiFi, cellular,
// or ethernet clients.
#include "config.h"

/************************ Example Starts Here *******************************/

// set up the 'example' feed
AdafruitIO_Feed *feed = io.feed("example");

// set up the 'example' dashboard
AdafruitIO_Dashboard *dashboard = io.dashboard("example");

void setup() {

  // start the serial connection
  Serial.begin(115200);

  // wait for serial monitor to open
  while(! Serial);

  // connect to io.adafruit.com
  Serial.print("Connecting to Adafruit IO");
  io.connect();

  // wait for a connection
  while(io.status() < AIO_CONNECTED) {
    Serial.print(".");
    delay(500);
  }

  // we are connected
  Serial.println();
  Serial.println(io.statusText());

  // create the example feed if it doesn't exist
  if(feed->exists()) {
    Serial.println("Example feed exists.");
  } else {
    if(feed->create()) {
      Serial.println("Example feed created.");
    } else {
      Serial.println("Example feed creation failed.");
    }
  }

  // create the example dashboard if it doesn't exist
  if(dashboard->exists()) {
    Serial.println("Example dashboard exists.");
  } else {
    if(dashboard->create()) {
      Serial.println("Example dashboard created.");
      // add blocks to the dashboard using the function below
      addBlocks();
    } else {
      Serial.println("Example dashboard creation failed.");
    }
  }

}

void loop() {

  // io.run(); is required for all sketches.
  // it should always be present at the top of your loop
  // function. it keeps the client connected to
  // io.adafruit.com, and processes any incoming data.
  io.run();

}

void addBlocks() {

  bool added = false;

  Serial.print("Adding momentary button block... ");
  MomentaryBlock *button = dashboard->addMomentaryBlock(feed);
  button->text = "Button";
  button->value = "1";
  button->release = "0";
  added = button->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding toggle button block... ");
  ToggleBlock *toggle = dashboard->addToggleBlock(feed);
  toggle->onText = "1";
  toggle->offText = "0";
  added = toggle->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding slider block... ");
  SliderBlock *slider = dashboard->addSliderBlock(feed);
  slider->min = 0;
  slider->max = 100;
  slider->step = 10;
  slider->label = "Value";
  added = slider->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding gauge block... ");
  GaugeBlock *gauge = dashboard->addGaugeBlock(feed);
  gauge->min = 0;
  gauge->max = 100;
  gauge->ringWidth = "thin"; // thin or thick
  gauge->label = "Value";
  added = gauge->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding line chart block... ");
  ChartBlock *chart = dashboard->addChartBlock(feed);
  chart->yAxisMin = 0;
  chart->yAxisMax = 100;
  chart->xAxisLabel = "X";
  chart->yAxisLabel = "Y";
  added = chart->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding text block... ");
  TextBlock *text = dashboard->addTextBlock(feed);
  text->fontSize = "small"; // small, medium, or large
  added = text->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding stream block... ");
  StreamBlock *stream = dashboard->addStreamBlock(feed);
  stream->fontSize = "small"; // small, medium, or large
  stream->fontColor = "green"; // green or white
  stream->showErrors = true;
  stream->showTimestamp = true;
  stream->showName = true;
  added = stream->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding color picker block... ");
  ColorBlock *color = dashboard->addColorBlock(feed);
  added = color->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding map block... ");
  MapBlock *map = dashboard->addMapBlock(feed);
  map->historyHours = 0;
  map->tile = "contrast"; // street, sat, or contrast
  added = map->save();
  Serial.println(added ? "added" : "failed");

  Serial.print("Adding image block... ");
  ImageBlock *image = dashboard->addImageBlock(feed);
  added = image->save();
  Serial.println(added ? "added" : "failed");

}
