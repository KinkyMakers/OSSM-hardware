#ifndef OSSM_SOFTWARE_OSSM_INTERFACE_H
#define OSSM_SOFTWARE_OSSM_INTERFACE_H

#include <Arduino.h>

#include "Events.h"  // for your event types like emergencyStop, home, etc.

class OSSMInterface {
  public:
    // State machine interface
    template <typename EventType>
    void process_event(const EventType& event);  // Ensure proper type handling

    virtual void ble_click(String command) = 0;
    virtual void moveTo(float intensity = 0,
                        uint16_t inTime = 0) = 0;  // intensity: 0-10

    // get current state
    virtual String getCurrentState() = 0;

    // target position
    uint16_t targetPosition;
    int16_t targetVelocity;
};

// Global pointer declaration
extern OSSMInterface* ossmInterface;

#endif
