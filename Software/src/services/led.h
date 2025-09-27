#ifndef OSSM_SOFTWARE_LED_H
#define OSSM_SOFTWARE_LED_H

#include <Arduino.h>
#include <FastLED.h>
#include "constants/Pins.h"

// LED configuration
#define NUM_LEDS 1
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Common colors
#define COLOR_OFF CRGB::Black
#define COLOR_RED CRGB::Red
#define COLOR_GREEN CRGB::Green
#define COLOR_BLUE CRGB::Blue
#define COLOR_WHITE CRGB::White
#define COLOR_YELLOW CRGB::Yellow
#define COLOR_PURPLE CRGB::Purple
#define COLOR_ORANGE CRGB::Orange
#define COLOR_CYAN CRGB::Cyan

// Status indication colors
#define COLOR_STATUS_IDLE CRGB::Blue
#define COLOR_STATUS_RUNNING CRGB::Green
#define COLOR_STATUS_ERROR CRGB::Red
#define COLOR_STATUS_HOMING CRGB::Yellow
#define COLOR_STATUS_CONNECTING CRGB::Purple

// Special effect colors
#define COLOR_DEEP_PURPLE CRGB(75, 0, 130)  // Deep purple for homing breathing

extern CRGB leds[NUM_LEDS];

// LED service functions
void initLED();
void setLEDColor(CRGB color);
void setLEDColor(uint8_t r, uint8_t g, uint8_t b);
void setLEDOff();
void setLEDBrightness(uint8_t brightness);
void flashLED(CRGB color, int duration_ms = 500);
void breatheLED(CRGB color, int period_ms = 2000);
void cycleLED(int period_ms = 3000);

// Status indication functions
void setLEDStatusIdle();
void setLEDStatusRunning();
void setLEDStatusError();
void setLEDStatusHoming();
void setLEDStatusConnecting();

// BLE status indication functions
void updateLEDForBLEStatus();
void showBLERainbow(int duration_ms = 1000);
void showBLEAdvertising();  // Breathing blue
void showBLEConnected();    // Dimmed blue
void showBLEDisconnected(); // Off

// Communication pulse functions
void pulseForCommunication();  // Brief brightness increase for BLE communication

// Machine status functions
void setHomingActive(bool active);
bool isHomingActive();
void updateLEDForMachineStatus();
void showHomingBreathing();  // Breathing deep purple

#endif // OSSM_SOFTWARE_LED_H