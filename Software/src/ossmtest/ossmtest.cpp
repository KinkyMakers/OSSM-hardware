#include "ossmtest.h"
#include "ossmtest/globalState.h"

void OSSMTEST::action() {
    sm2->process_event(event_trigger{});
}
