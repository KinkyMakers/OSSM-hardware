#ifndef OSSM_SOFTWARE_OSSM_INTERFACE_H
#define OSSM_SOFTWARE_OSSM_INTERFACE_H

#include <Arduino.h>

#include "Events.h"  // for your event types like emergencyStop, home, etc.

class OSSMInterface {
  public:
    // State machine interface
    template <typename EventType>
    void process_event(const EventType& event);  // Ensure proper type handling

    // get current state
    String get_current_state();
};

// Global pointer declaration
extern OSSMInterface* ossmInterface;

#endif