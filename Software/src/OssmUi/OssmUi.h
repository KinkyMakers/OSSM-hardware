
#ifndef OSSM_UI_H
#define OSSM_UI_H

#include <Arduino.h>

#include "services/display.h"

class OssmUi
{
   public:
    void Setup();

    void UpdateState(String mode_label, const int speed_percentage, const int encoder_position);

    void UpdateMessage(String message_in);
};

#endif
