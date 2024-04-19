
#ifndef OSSM_UI_H
#define OSSM_UI_H

#include <Arduino.h>

#include "services/display.h"

class OssmUi {
  public:
    void Setup();

    static void UpdateState(const String& mode_label, int speed_percentage,
                            int encoder_position);

    static void UpdateMessage(const String& message_in);
};

#endif
