#include "state.h"

#include "../Events.h"

StateLogger stateLogger;

// Static pointer to hold the state machine instance
sml::sm<OSSMStateMachine, sml::thread_safe<ESP32RecursiveMutex>,
        sml::logger<StateLogger>> *stateMachine = nullptr;

void initStateMachine() {
    if (stateMachine == nullptr) {
        stateMachine = new sml::sm<OSSMStateMachine,
                                   sml::thread_safe<ESP32RecursiveMutex>,
                                   sml::logger<StateLogger>>(stateLogger);

        stateMachine->process_event(Done{});
    }
}
