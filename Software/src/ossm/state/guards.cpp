#include "guards.h"

#include "guard_logic.h"
#include "constants/Config.h"
#include "constants/Pins.h"
#include "ossm/homing/homing.h"
#include "ossm/state/calibration.h"
#include "ossm/state/menu.h"
#include "utils/analog.h"

bool ossmIsStrokeTooShort() {
    return homing::isStrokeTooShort();
}

bool ossmIsNotHomed() {
    return calibration.isHomed == false;
}

bool ossmIsPreflightSafe() {
    float potReading =
        getAnalogAveragePercent({Pins::Remote::speedPotPin, 50});
    return guard_logic::isPreflightSafeLogic(
        potReading, Config::Advanced::commandDeadZonePercentage);
}

Menu ossmGetMenuOption() {
    return menuState.currentOption;
}
