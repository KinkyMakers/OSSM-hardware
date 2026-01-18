#ifndef OSSM_SOFTWARE_BOARD_H
#define OSSM_SOFTWARE_BOARD_H

#include <Arduino.h>
#include <NimBLEDevice.h>

#include "constants/Pins.h"
#include "services/encoder.h"
#include "services/led.h"
#include "services/stepper.h"

extern bool USE_SPEED_KNOB_AS_LIMIT;
void initBoard();

#endif  // OSSM_SOFTWARE_BOARD_H
