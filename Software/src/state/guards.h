//
// Created by Andrew Koenig on 2023-12-14.
//

#ifndef SOFTWARE_GUARDS_H
#define SOFTWARE_GUARDS_H

#include "Utilities.h"
// Guard definitions
auto isPreflightSafe = [](OSSM &o) {
    // TODO: Implement this
    return o.speedPercentage == 0;
};
auto isOption = [](Menu option) {
    // TODO: Implement this
    return [option]() { return option && true; };
};

#endif // SOFTWARE_GUARDS_H
