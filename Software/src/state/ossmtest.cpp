#include "ossmtest.h"
#include "state/globalState.h"

void OSSMTEST::action() {
    sm2->process_event(event_trigger{});
}
