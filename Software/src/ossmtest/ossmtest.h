#ifndef SOFTWARE_OSSMTEST_H
#define SOFTWARE_OSSMTEST_H

#include "globalstate.h"
#include "ossmi.h"
#include "ossmtest/globalstate.h"
class OSSMTEST : public OSSMI {
  public:
    void action() override {
        // do something
        sm2.process_event(Done{});
    }
};

#endif  // SOFTWARE_OSSMTEST_H
