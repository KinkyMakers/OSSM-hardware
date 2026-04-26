#include "guards.h"

#include "constants/Config.h"
#include "constants/Pins.h"
#include "ossm/homing/homing.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "utils/AnalogSampler.h"

bool ossmIsStrokeTooShort() {
    return homing::isStrokeTooShort();
}

bool ossmIsNotHomed() {
    return calibration.isHomed == false;
}

bool ossmIsPreflightSafe() {
    return AnalogSampler::readPercent(Pins::Remote::speedPotPin) <
           Config::Advanced::commandDeadZonePercentage;
}

Menu ossmGetMenuOption() {
    return menuState.currentOption;
}
